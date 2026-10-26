import { useEffect, useId, useMemo, useRef, useState } from 'react';
import './TextLoop.css';

const VIEW_W = 1200;
const VIEW_H = 260;
const CX = VIEW_W / 2;
const CY = VIEW_H / 2;
const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

function buildPath(shape, curviness, suppliedPath) {
  if (suppliedPath) return suppliedPath;
  const amount = clamp(Math.abs(curviness), 0, 90);
  const direction = curviness < 0 ? -1 : 1;
  const amplitude = Math.min(30 + amount * 0.85, 98) * direction;
  if (shape === 'line') return `M -400 ${CY} L ${VIEW_W + 400} ${CY}`;
  if (shape === 'arch') return `M -100 ${CY + amplitude * 0.3} Q ${CX} ${CY - amplitude} ${VIEW_W + 100} ${CY + amplitude * 0.3}`;
  if (shape === 'circle') {
    const rx = 330 + amount * 1.2;
    const ry = 70 + amount * 0.3;
    return `M ${CX - rx} ${CY} A ${rx} ${ry} 0 1 1 ${CX + rx} ${CY} A ${rx} ${ry} 0 1 1 ${CX - rx} ${CY}`;
  }
  if (shape === 'infinity') {
    const rx = 390;
    const ry = 35 + amount * 0.55;
    return `M ${CX} ${CY} C ${CX + rx * 0.6} ${CY - ry} ${CX + rx} ${CY - ry} ${CX + rx} ${CY} C ${CX + rx} ${CY + ry} ${CX + rx * 0.6} ${CY + ry} ${CX} ${CY} C ${CX - rx * 0.6} ${CY - ry} ${CX - rx} ${CY - ry} ${CX - rx} ${CY} C ${CX - rx} ${CY + ry} ${CX - rx * 0.6} ${CY + ry} ${CX} ${CY}`;
  }
  return `M -350 ${CY} Q -175 ${CY - amplitude} 0 ${CY} T 350 ${CY} T 700 ${CY} T 1050 ${CY} T ${VIEW_W + 350} ${CY}`;
}

/** Smooth, requestAnimationFrame-driven SVG text loop. */
export default function TextLoop({
  text = 'YLang · Open source · Native builds',
  shape = 'wave',
  path,
  speed = 90,
  direction = 'forward',
  separator = '·',
  curviness = 34,
  tilt = 0,
  fontSize = 46,
  fontWeight = 800,
  letterSpacing = 2,
  uppercase = true,
  color,
  ribbon = true,
  ribbonColor = '#167d8d',
  ribbonWidth = 70,
  edgeFade = 0,
  pauseOnHover = true,
  cross = false,
  crossColor,
  crossAngle = 16,
  draggable = false,
  scrollBoost = 0,
  outline = false,
  outlineWidth = 1.5,
  className = '',
  style = {}
}) {
  const rootRef = useRef(null);
  const pathRef = useRef(null);
  const measureRef = useRef(null);
  const headRef = useRef(null);
  const tailRef = useRef(null);
  const pausedRef = useRef(false);
  const progressRef = useRef(0);
  const [metrics, setMetrics] = useState({ length: 0, reps: 2 });
  const rawId = useId().replace(/[^a-zA-Z0-9_-]/g, '');
  const pathId = `ylang-text-loop-${rawId}`;
  const fadeId = `ylang-text-loop-fade-${rawId}`;
  const d = useMemo(() => buildPath(shape, curviness, path), [shape, curviness, path]);
  const base = uppercase ? String(text).toUpperCase() : String(text);
  const unit = `${base}${separator ? `\u00a0${separator}\u00a0` : '\u00a0\u00a0\u00a0'}`;
  const repeatedText = unit.repeat(Math.max(2, metrics.reps));
  const fadeWidth = clamp(edgeFade, 0, 1) * 30;
  const crossD = useMemo(() => buildPath(shape, -curviness, path), [shape, curviness, path]);

  useEffect(() => {
    const pathElement = pathRef.current;
    const measureElement = measureRef.current;
    if (!pathElement || !measureElement) return undefined;
    let cancelled = false;
    const measure = () => {
      if (cancelled) return;
      try {
        const length = pathElement.getTotalLength();
        const width = measureElement.getComputedTextLength();
        if (!length) return;
        // Fill the path with a slightly shorter string, so SVG only expands gaps
        // to fit. Overfilling it can collapse spacing and stick letters together.
        const reps = width > 0 ? clamp(Math.floor(length / width), 1, 80) : 2;
        setMetrics(previous => previous.length === length && previous.reps === reps ? previous : { length, reps });
      } catch {
        // SVG measurements can briefly be unavailable during the first paint.
      }
    };
    measure();
    document.fonts?.ready?.then(measure).catch(() => {});
    return () => { cancelled = true; };
  }, [d, unit, fontSize, fontWeight, letterSpacing]);

  useEffect(() => {
    const root = rootRef.current;
    const head = headRef.current;
    const tail = tailRef.current;
    const pathElement = pathRef.current;
    if (!root || !head || !tail || !pathElement || !metrics.length || speed <= 0) return undefined;
    const reducedQuery = window.matchMedia?.('(prefers-reduced-motion: reduce)');
    if (reducedQuery?.matches) return undefined;

    let raf = 0;
    let last = 0;
    let visible = true;
    let disposed = false;
    let offset = progressRef.current * metrics.length;
    const sign = direction === 'reverse' ? -1 : 1;
    const apply = () => {
      const current = ((offset % metrics.length) + metrics.length) % metrics.length;
      progressRef.current = current / metrics.length;
      head.setAttribute('startOffset', String(current));
      tail.setAttribute('startOffset', String(current - metrics.length));
    };
    const frame = now => {
      raf = 0;
      if (disposed || !visible || document.hidden) return;
      if (last) offset += sign * speed * Math.min(0.05, Math.max(0, (now - last) / 1000)) * (pausedRef.current ? 0 : 1);
      last = now;
      apply();
      raf = requestAnimationFrame(frame);
    };
    const wake = () => {
      if (!raf && !disposed && visible && !document.hidden) raf = requestAnimationFrame(frame);
    };
    const onEnter = () => { if (pauseOnHover) pausedRef.current = true; };
    const onLeave = () => { pausedRef.current = false; wake(); };
    const onVisibility = () => {
      last = 0;
      if (document.hidden) {
        if (raf) cancelAnimationFrame(raf);
        raf = 0;
      } else wake();
    };
    const observer = typeof IntersectionObserver === 'undefined' ? null : new IntersectionObserver(entries => {
      visible = entries.some(entry => entry.isIntersecting);
      if (visible) { last = 0; wake(); }
      else { if (raf) cancelAnimationFrame(raf); raf = 0; }
    }, { threshold: 0.01 });
    observer?.observe(root);
    root.addEventListener('pointerenter', onEnter);
    root.addEventListener('pointerleave', onLeave);
    document.addEventListener('visibilitychange', onVisibility);
    apply();
    wake();
    return () => {
      disposed = true;
      if (raf) cancelAnimationFrame(raf);
      observer?.disconnect();
      root.removeEventListener('pointerenter', onEnter);
      root.removeEventListener('pointerleave', onLeave);
      document.removeEventListener('visibilitychange', onVisibility);
    };
  }, [metrics.length, speed, direction, pauseOnHover]);

  const fill = color || (ribbon ? '#ffffff' : 'currentColor');
  const textPaint = outline
    ? { fill: 'none', stroke: fill, strokeWidth: outlineWidth, strokeLinejoin: 'round' }
    : { fill };
  const fadeStyle = fadeWidth > 0 ? {
    maskImage: `linear-gradient(90deg, transparent, #000 ${fadeWidth}%, #000 ${100 - fadeWidth}%, transparent)`,
    WebkitMaskImage: `linear-gradient(90deg, transparent, #000 ${fadeWidth}%, #000 ${100 - fadeWidth}%, transparent)`
  } : undefined;
  const transform = tilt ? `rotate(${tilt} ${CX} ${CY})` : undefined;

  return (
    <div ref={rootRef} className={`text-loop${draggable ? ' text-loop--draggable' : ''}${className ? ` ${className}` : ''}`} style={{ ...style, ...fadeStyle }}>
      <svg className="text-loop-svg" viewBox={`0 0 ${VIEW_W} ${VIEW_H}`} preserveAspectRatio="xMidYMid meet" role="img" aria-label={text}>
        <defs>
          <path ref={pathRef} id={pathId} d={d} fill="none" />
          {cross && <path id={`${pathId}-cross`} d={crossD} fill="none" />}
          {edgeFade > 0 && <linearGradient id={fadeId} x1="0" y1="0" x2="1" y2="0"><stop offset="0" stopColor="white" stopOpacity="0" /><stop offset={`${fadeWidth}%`} stopColor="white" stopOpacity="1" /><stop offset={`${100 - fadeWidth}%`} stopColor="white" stopOpacity="1" /><stop offset="1" stopColor="white" stopOpacity="0" /></linearGradient>}
        </defs>
        <text ref={measureRef} className="text-loop-measure" style={{ fontSize, fontWeight, letterSpacing }} aria-hidden="true">{unit}</text>
        {ribbon && <path d={d} transform={transform} fill="none" stroke={ribbonColor} strokeWidth={ribbonWidth} strokeLinecap="round" strokeLinejoin="round" />}
        {cross && ribbon && <path d={crossD} fill="none" stroke={crossColor || '#d6eee8'} strokeWidth={ribbonWidth * 0.65} strokeLinecap="round" opacity="0.6" />}
        <g transform={transform}>
          <text className="text-loop-text" style={{ fontSize, fontWeight, letterSpacing }} {...textPaint} dominantBaseline="central" textLength={metrics.length || undefined} lengthAdjust="spacing">
            <textPath ref={headRef} href={`#${pathId}`} startOffset={0}>{repeatedText}</textPath>
          </text>
          <text className="text-loop-text" style={{ fontSize, fontWeight, letterSpacing }} {...textPaint} dominantBaseline="central" textLength={metrics.length || undefined} lengthAdjust="spacing">
            <textPath ref={tailRef} href={`#${pathId}`} startOffset={0}>{repeatedText}</textPath>
          </text>
        </g>
      </svg>
    </div>
  );
}
