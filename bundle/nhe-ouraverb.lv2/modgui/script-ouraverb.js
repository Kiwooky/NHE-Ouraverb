function (event, funcs) {
    // Ouraverb face script. ES5 and small on purpose: if a face script throws,
    // mod-ui silently disables it.
    //  - OU steps Place; the readout shows it (pre-rendered strip, no web font).
    //  - Ceiling cover / Cover density sliders: drag or click along the track.
    //  - Orbital display, bottom to top:
    //      cover:    size = Ceiling cover (0..100 %), opacity = Cover density (0..10 -> 10..100 %)
    //      dynamics: opacity = the plugin's Input Level output (as Trance Atlantic)
    //      height:   opacity = (Height knob position)^3, so it darkens most near the top
    // mod-ui doesn't echo a script's own set_port_value back, so we redraw after setting.
    var icon = event.icon;
    var d = event.data;
    var NPLACE = 5, FRAME = 160;              // readout frame width, CSS px
    var CAP_MIN = 61, CAP_TRAVEL = 205, CAP_HALF = 47.5;   // slider cap in the track, CSS px
    var RANGE = { cover: 100, cover_density: 10 };

    function clamp01(x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
    function num(v, def) { v = parseFloat(v); return isNaN(v) ? def : v; }
    function scale(el, s) { el.css({ transform: 'scale(' + s + ')', '-webkit-transform': 'scale(' + s + ')' }); }

    function draw(sym) {
        var all = !sym;
        if (all || sym == 'place') {
            var p = Math.max(0, Math.min(NPLACE - 1, Math.round(num(d.place, 0))));
            icon.find('.ov-text').css('background-position', (-FRAME * p) + 'px 0');
        }
        if (all || sym == 'cover' || sym == 'cover_density') {
            var c = clamp01(num(d.cover, 0) / 100), k = clamp01(num(d.cover_density, 3) / 10);
            var cov = icon.find('.ov-orbit-cover');
            scale(cov, c);
            cov.css('opacity', c > 0 ? 0.1 + 0.9 * k : 0);   // never fully clear while it's out
            icon.find('[ov-slider="cover"] .ov-cap').css('left', (CAP_MIN + CAP_TRAVEL * c) + 'px');
            icon.find('[ov-slider="cover_density"] .ov-cap').css('left', (CAP_MIN + CAP_TRAVEL * k) + 'px');
        }
        if (all || sym == 'cover_height') {
            var h = Math.max(1, Math.min(60, num(d.cover_height, 20)));
            var pos = clamp01(Math.log(h) / Math.log(60));          // the knob's position
            icon.find('.ov-orbit-height').css('opacity', pos * pos * pos);   // darkens most near the top
        }
        if (all || sym == 'input_level') {
            icon.find('.ov-orbit-dynamics').css('opacity', clamp01(num(d.input_level, 0)));
        }
    }
    function set(sym, v) {
        d[sym] = v;
        funcs.set_port_value(sym, v);
        draw(sym);
    }
    function pageX(e) {
        var o = e.originalEvent || e;
        if (o.touches && o.touches.length) return o.touches[0].pageX;
        if (o.changedTouches && o.changedTouches.length) return o.changedTouches[0].pageX;
        return e.pageX;
    }

    if (event.type == 'start') {
        var ports = event.ports || [];
        for (var i = 0; i < ports.length; i++) d[ports[i].symbol] = ports[i].value;
        draw();

        // OU: act on press; ignore the emulated mouse events that follow a touch
        var btn = icon.find('[ov-step]');
        function step(e) {
            e.preventDefault(); e.stopPropagation();
            if (e.type == 'touchstart') d.lastTouch = new Date().getTime();
            else if (d.lastTouch && new Date().getTime() - d.lastTouch < 1000) return;
            btn.addClass('down');
            set('place', (Math.round(num(d.place, 0)) + 1) % NPLACE);
        }
        btn.on('touchstart mousedown', step);
        btn.on('click', function (e) { e.preventDefault(); e.stopPropagation(); });
        btn.on('mouseup mouseleave touchend touchcancel', function () { btn.removeClass('down'); });

        // sliders: the cap's centre follows the pointer; the panel may be zoomed, so
        // measure the track as drawn
        icon.find('[ov-slider]').each(function () {
            var tr = jQuery(this), sym = tr.attr('ov-slider');
            function apply(e) {
                var w = tr[0].getBoundingClientRect().width || 357, z = w / 357;
                var x = (pageX(e) - tr.offset().left) / z - CAP_HALF;
                var v = clamp01((x - CAP_MIN) / CAP_TRAVEL) * RANGE[sym];
                v = Math.round(v * 10) / 10;
                if (v != d[sym]) set(sym, v);
            }
            // a press sets the value where you press; moving only drags once past 3 px,
            // so a slightly wobbly click or tap lands where it was aimed
            var x0 = 0, dragging = false;
            function move(e) {
                e.preventDefault();
                if (!dragging && Math.abs(pageX(e) - x0) < 3) return;
                dragging = true;
                apply(e);
            }
            function up() { jQuery(document).off('.ovdrag'); }
            tr.on('touchstart mousedown', function (e) {
                e.preventDefault(); e.stopPropagation();
                if (e.type == 'touchstart') d.lastTouch = new Date().getTime();
                else if (d.lastTouch && new Date().getTime() - d.lastTouch < 1000) return;
                x0 = pageX(e); dragging = false;
                apply(e);
                jQuery(document).on('mousemove.ovdrag touchmove.ovdrag', move)
                                .on('mouseup.ovdrag touchend.ovdrag touchcancel.ovdrag', up);
            });
            tr.on('click', function (e) { e.preventDefault(); e.stopPropagation(); });
        });
    } else if (event.type == 'change') {
        d[event.symbol] = event.value;
        draw(event.symbol);
    }
}
