#!/usr/bin/env python3
"""Build a one-file, in-browser test bench for the Ouraverb face.

    python3 tools/face_prototype.py bundle/nhe-ouraverb.lv2 out.html [--mod-ui <checkout>]

The page runs the real face files (template, stylesheet, script, images) with
mod-ui's own widget code (html/js/modgui.js), bound the way mod-ui binds them,
as tools/face_click_test.py does. Around it: simulated playing that drives the
Input Level output (Trance Atlantic's follower: instant rise, ~180 ms fall,
-48..-12 dBFS -> 0..1, ^1.5, sent about 10 times a second like the MOD), the
factory presets, and live readouts of every control and orbit layer.
Everything is inlined (images as data: URIs, mod-ui's jQuery 1.9.1), so the page has no outside dependencies
besides its web fonts.
"""
import base64, json, mimetypes, os, re, subprocess, sys
import rdflib

L = rdflib.Namespace('http://lv2plug.in/ns/lv2core#')
MG = rdflib.Namespace('http://moddevices.com/ns/modgui#')
PS = rdflib.Namespace('http://lv2plug.in/ns/ext/presets#')
RDF, RDFS = rdflib.RDF, rdflib.RDFS


def main():
    bundle, out = sys.argv[1].rstrip('/'), sys.argv[2]
    modui = sys.argv[sys.argv.index('--mod-ui') + 1] if '--mod-ui' in sys.argv else \
        os.path.join(os.environ.get('PB_CACHE', os.path.expanduser('~/.cache/mod-playbook')), 'mod-ui')
    if not os.path.isdir(modui):
        subprocess.run(['git', 'clone', '-q', '--depth', '1', 'https://github.com/mod-audio/mod-ui.git', modui], check=True)

    g = rdflib.Graph()
    for fn in os.listdir(bundle):
        if fn.endswith('.ttl'):
            g.parse(os.path.join(bundle, fn), format='turtle')
    gui = next(g.objects(None, MG['gui']))
    rel = lambda k: os.path.join(bundle, str(g.value(gui, MG[k])).replace('file://', ''))
    res = rel('resourcesDirectory')

    ports, names = {}, {}
    for p in g.objects(None, L['port']):
        sym = g.value(p, L['symbol'])
        if sym is None or (p, RDF.type, L['ControlPort']) not in g:
            continue
        names[str(sym)] = str(g.value(p, L['name']))
        if (p, RDF.type, L['InputPort']) not in g or g.value(p, L['designation']) is not None:
            continue
        ports[str(sym)] = {
            'symbol': str(sym),
            'ranges': {'minimum': float(g.value(p, L['minimum'])), 'maximum': float(g.value(p, L['maximum'])),
                       'default': float(g.value(p, L['default']))},
            'properties': [str(o).split('#')[-1] for o in g.objects(p, L['portProperty'])],
            'scalePoints': sorted(({'value': float(g.value(s, RDF.value)), 'label': str(g.value(s, RDFS.label))}
                                   for s in g.objects(p, L['scalePoint'])), key=lambda d: d['value']),
        }
    # the face's own names for the controls (modgui:port), else the TTL's
    for mp in g.objects(gui, MG['port']):
        names[str(g.value(mp, L['symbol']))] = str(g.value(mp, L['name']))
    presets = []
    for pr in g.subjects(RDF.type, PS.Preset):
        vals = {str(g.value(n, L.symbol)): float(g.value(n, PS.value)) for n in g.objects(pr, L['port'])}
        if vals:
            presets.append({'label': str(g.value(pr, RDFS.label)), 'values': vals})
    presets.sort(key=lambda p: p['label'])

    def data_uri(m):
        f = os.path.join(res, m.group(1))
        mime = mimetypes.guess_type(f)[0] or 'application/octet-stream'
        return 'url(data:%s;base64,%s)' % (mime, base64.b64encode(open(f, 'rb').read()).decode())

    css = open(rel('stylesheet')).read().replace('{{{cns}}}', '').replace('{{{ns}}}', '')
    css = re.sub(r'url\(/resources/([^)]+)\)', data_uri, css)
    html = open(rel('iconTemplate')).read()
    html = re.sub(r'\{\{#.*?\}\}.*?\{\{/.*?\}\}', '', html, flags=re.S).replace('{{{cns}}}', '').replace('{{{ns}}}', '')
    script = open(rel('javascript')).read()
    modgui = open(os.path.join(modui, 'html', 'js', 'modgui.js')).read()
    label = str(g.value(gui, MG['label']))

    page = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'face_prototype.html')).read()
    page = (page.replace('/*@FACE_CSS@*/', css)
                .replace('<!--@FACE_HTML@-->', html)
                .replace('/*@MODGUI_JS@*/', modgui.replace('</script', '<\\/script'))
                .replace('/*@JQUERY_JS@*/', open(os.path.join(modui, 'html', 'js', 'lib', 'jquery-1.9.1.min.js')).read())
                .replace('"@PORTS@"', json.dumps(ports))
                .replace('"@NAMES@"', json.dumps(names))
                .replace('"@PRESETS@"', json.dumps(presets, ensure_ascii=False))
                .replace('"@SCRIPT@"', json.dumps(script).replace('</', '<\\/'))
                .replace('@LABEL@', label))
    open(out, 'w', encoding='utf-8').write(page)
    print('wrote %s (%d KB)' % (out, os.path.getsize(out) // 1024))


if __name__ == '__main__':
    main()
