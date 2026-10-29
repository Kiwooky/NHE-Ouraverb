#!/usr/bin/env python3
"""Tests for Ouraverb Stereo (2 in, 2 out). Run with tools/check_stereo.sh.

The promise: it is two mono Ouraverbs, the left input's making the left output and the
right input's the right. So the left output must not depend on the right input at all (and
the other way round), exactly, at any setting. That the halves are the mono plugin's is
checked against a mono build of the same machine when one is given (PB_SO_MONO for the native
run, PB_SO_MONO_ARM for the Duo build under qemu; the same host runs both plugins): to
within one step of a 24-bit converter (the one-sided loops compile to slightly different float
rounding, a last-bit difference around -145 dBFS).
The DSP itself is tested in test_nhe_ouraverb.py."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
for d in (os.environ.get('PB_TOOLS', ''), os.path.join(HERE, '..', 'tools')):
    if d and os.path.exists(os.path.join(d, 'lv2test.py')):
        sys.path.insert(0, d); break
import numpy as np
from lv2test import Plugin, check, done, db, rms, pluck_train, tick_count, max_step

ORDER = ['place', 'expanse', 'surface', 'air', 'bounce', 'cover', 'cover_density', 'cover_height', 'mix', 'tone', 'tails',
         'input_level', 'lv2_enabled']
DEF = dict(place=0, expanse=100, surface=8, air=5, bounce=4, cover=0, cover_density=3, cover_height=20, mix=35, tone=5,
           tails=1, input_level=0, lv2_enabled=1)
p = Plugin(order=ORDER, defaults=DEF, n_in=2, n_out=2)
sr = 48000
PLACES = ['Fjord', 'Canyon', 'Valley', 'Forest', 'Colosseum']

def st(a, b):
    return np.stack([a, b], axis=1)

xa = pluck_train(sr, 8.0)
xb = pluck_train(sr, 8.0, notes=(330, 147, 262, 196, 392, 110), every=1.1)
z = np.zeros_like(xa)

# --- the promise: each output hears only its own input --------------------------------
CASES = [dict(place=s) for s in range(5)] + [
    dict(place=4, cover=60, surface=3, tone=8), dict(place=3, bounce=9, air=2, tone=1),
    dict(place=1, expanse=40, cover=100, cover_height=2), dict(place=0, tone=10, bounce=10, surface=10)]
for kw in CASES:
    y1 = p.run(st(xa, xb), sr, **kw)
    y2 = p.run(st(xa, z), sr, **kw)
    y3 = p.run(st(z, xb), sr, **kw)
    check('left out ignores the right input, right out ignores the left, exactly: %s' % kw,
          np.array_equal(y1[:, 0], y2[:, 0]) and np.array_equal(y1[:, 1], y3[:, 1]),
          'max diff L %.2g, R %.2g' % (np.abs(y1[:, 0] - y2[:, 0]).max(), np.abs(y1[:, 1] - y3[:, 1]).max()))
ev = [(2 * sr, 'place', 2), (4 * sr, 'expanse', 30), (5 * sr, 'tone', 9), (6 * sr, 'lv2_enabled', 0)]
y1 = p.run(st(xa, xb), sr, events=ev); y2 = p.run(st(xa, z), sr, events=ev); y3 = p.run(st(z, xb), sr, events=ev)
check('...also through live changes and bypass', np.array_equal(y1[:, 0], y2[:, 0]) and np.array_equal(y1[:, 1], y3[:, 1]), '')

# --- the same signal on both inputs is the mono plugin (needs a mono build) ---------------
on_duo = 'qemu-arm' in os.environ.get('PB_HOST', '')
mono_so = os.environ.get('PB_SO_MONO_ARM' if on_duo else 'PB_SO_MONO')
if mono_so:
    O1 = ORDER
    LSB24 = 2.0 ** -23   # one step of a 24-bit converter, full scale +-1
    same = lambda a, b: np.abs(a - b).max() < LSB24
    m = Plugin(order=O1, defaults=DEF, n_in=1, n_out=2, so=mono_so)
    for kw in CASES[:6]:
        ym = m.run(xa, sr, **kw); ys = p.run(st(xa, xa), sr, **kw)
        check('same signal on both inputs = the mono plugin, within a 24-bit step: %s' % kw, same(ym, ys),
              'max diff %.2g' % np.abs(ym - ys).max())
    ym = m.run(xa, sr, **CASES[5]); yn = m.run(xb, sr, **CASES[5]); ys = p.run(st(xa, xb), sr, **CASES[5])
    check('two inputs = left out of a mono fed L, right out of a mono fed R (the two-instance setup)',
          same(ys[:, 0], ym[:, 0]) and same(ys[:, 1], yn[:, 1]),
          'max diff %.2g' % max(np.abs(ys[:, 0] - ym[:, 0]).max(), np.abs(ys[:, 1] - yn[:, 1]).max()))
else:
    print('(%s not set: the comparison with the mono build was skipped)' % ('PB_SO_MONO_ARM' if on_duo else 'PB_SO_MONO'))

# --- [every] levels, ticks, bypass, torture, silence --------------------------------------
yb = p.run(st(xa, xb), sr, lv2_enabled=0)
for s_, nm in enumerate(PLACES):
    y = p.run(st(xa, xb), sr, place=s_)
    d = db(rms(y[sr:]) / rms(yb[sr:]))
    check('%s defaults: within +-3 dB of bypass, no ticks' % nm,
          abs(d) < 3 and tick_count(y[:, 0], sr) == 0 and tick_count(y[:, 1], sr) == 0, '%+.1f dB' % d)

x = 0.1 * np.sin(2 * np.pi * 220 * np.arange(int(3 * sr)) / sr) * np.minimum(1, np.arange(int(3 * sr)) / (0.05 * sr))
x2 = 0.1 * np.sin(2 * np.pi * 330 * np.arange(int(3 * sr)) / sr) * np.minimum(1, np.arange(int(3 * sr)) / (0.05 * sr))
for tails in (1, 0):
    y = p.run(st(x, x2), sr, place=3, tails=tails, events=[(sr, 'lv2_enabled', 0)])
    a, b = int(1.3 * sr), int(2.9 * sr)
    step = max(max_step(y[int(0.98 * sr):int(1.05 * sr), c]) for c in (0, 1))
    ok = True if tails else (np.abs(y[a:b, 0] - x[a:b]).max() < 1e-4 and np.abs(y[a:b, 1] - x2[a:b]).max() < 1e-4)
    check('bypass tails=%d: %s, no click' % (tails, 'tail rings out' if tails else 'each dry at unity on its own side'),
          ok and step < 1.5 * max(max_step(x), max_step(x2)), 'max step %.4f' % step)

for srt in (44100, 48000, 96000):
    n = np.random.default_rng(1).standard_normal((int(3 * srt), 2)) * 0.8
    for kw in (dict(place=4, expanse=200, surface=10, air=10, bounce=10, cover=100, cover_height=1, cover_density=10, mix=100, tone=10),
               dict(place=3, expanse=2, surface=0, air=0, bounce=0, cover=0, cover_height=60, cover_density=0, mix=0, tone=0)):
        y = p.run(n, srt, **kw)
        check('torture %d Hz %s' % (srt, 'max' if kw['bounce'] else 'min'),
              np.isfinite(y).all() and (kw['mix'] == 0 or np.abs(y).max() < 1.0), 'peak %.2f' % np.abs(y).max())

_, lv = p.run(st(z[:4 * sr], pluck_train(sr, 4.0)), sr, trace='input_level')
check('Input Level follows the louder input (plucks on the right only)', 0.5 < np.asarray(lv).max() < 0.9,
      'peak %.2f' % np.asarray(lv).max())

y = p.run(np.zeros((sr, 2)), sr)
check('silence', np.abs(y).max() < 1e-6)

done()
