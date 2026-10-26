import { memo, useEffect, useMemo, useRef, useState } from 'react';
import './LogoLoop.css';

const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
const toCssLength = value => typeof value === 'number' ? `${value}px` : (value ?? '100%');

/**
 * A lightweight, accessible LogoLoop integration based on the React Bits API.
 * Supports icon nodes or image marks, seamless repetition, hover states,
 * labels, fading, vertical direction and reduced-motion preferences.
 */
const LogoLoop = memo(function LogoLoop({
  logos = [],
  speed = 120,
  direction = 'left',
  width = '100%',
  logoHeight = 28,
  gap = 32,
  pauseOnHover,
  hoverSpeed,
  fadeOut = true,
  fadeOutWidth = 0.12,
  scaleOnHover = false,
  layout = 'line',
  bend = 0.5,
  focus = 0,
  magnify = 0,
  muted = 0,
  labels = 'none',
  draggable = true,
  intro = true,
  renderItem,
  ariaLabel = 'Partner logos',
  className = '',
  style = {}
}) {
  const rootRef = useRef(null);
  const trackRef = useRef(null);
  const dragRef = useRef(null);
  const [dragOffset, setDragOffset] = useState(0);
  const [reducedMotion, setReducedMotion] = useState(false);
  const [visible, setVisible] = useState(true);
  const vertical = direction === 'up' || direction === 'down';
  const safeSpeed = Math.max(0, Math.abs(Number(speed) || 0));
  const itemPitch = Math.max(logoHeight * 2.45, 120) + gap;
  const period = Math.max(itemPitch * logos.length, itemPitch);
  const duration = safeSpeed === 0 ? 0 : Math.max(4, period / safeSpeed);
  const effectiveHoverSpeed = hoverSpeed !== undefined ? Math.abs(hoverSpeed) : pauseOnHover === false ? null : 0;
  const hoverDuration = effectiveHoverSpeed === null || effectiveHoverSpeed === 0 || safeSpeed === 0
    ? duration
    : Math.max(4, period / effectiveHoverSpeed);
  const mutedAmount = clamp(muted, 0, 1);
  const maskSize = `${clamp(fadeOutWidth, 0, 0.5) * 100}%`;
  const pausesOnHover = effectiveHoverSpeed === 0;
  const isReverse = direction === 'right' || direction === 'down';

  useEffect(() => {
    const query = window.matchMedia('(prefers-reduced-motion: reduce)');
    const update = () => setReducedMotion(query.matches);
    update();
    query.addEventListener?.('change', update);
    const observer = typeof IntersectionObserver === 'undefined' ? null : new IntersectionObserver(([entry]) => {
      setVisible(entry.isIntersecting);
    }, { threshold: 0.05 });
    if (rootRef.current && observer) observer.observe(rootRef.current);
    return () => {
      query.removeEventListener?.('change', update);
      observer?.disconnect();
    };
  }, []);

  const copies = useMemo(() => [0, 1], []);
  const rootStyle = {
    width: toCssLength(width),
    '--logoloop-gap': `${gap}px`,
    '--logoloop-logo-height': `${logoHeight}px`,
    '--logoloop-duration': `${duration || 0}s`,
    '--logoloop-hover-duration': `${hoverDuration || 0}s`,
    '--logoloop-fade-width': maskSize,
    '--logoloop-muted': String(1 - mutedAmount * 0.6),
    '--logoloop-gray': String(Math.min(1, mutedAmount * 2)),
    '--logoloop-bend': String(clamp(bend, -1, 1) * 22),
    '--logoloop-drag-offset': `${dragOffset}px`,
    ...(style || {})
  };

  return (
    <div
      ref={rootRef}
      className={`logoloop ${vertical ? 'logoloop--vertical' : 'logoloop--horizontal'} ${scaleOnHover ? 'logoloop--scale-hover' : ''} ${className}`.trim()}
      style={rootStyle}
      role="region"
      aria-label={ariaLabel}
      data-direction={direction}
      data-layout={layout}
      data-muted={mutedAmount > 0 ? '' : undefined}
      data-labels={labels !== 'none' ? labels : undefined}
      data-fade={fadeOut ? '' : undefined}
      data-draggable={draggable ? '' : undefined}
      data-intro={intro ? '' : undefined}
      data-reduced={reducedMotion ? '' : undefined}
      data-visible={visible ? '' : undefined}
      data-hover-pauses={pausesOnHover ? '' : undefined}
      data-no-hover-pause={effectiveHoverSpeed === null ? '' : undefined}
      onPointerDown={event => {
        if (!draggable || (event.pointerType === 'mouse' && event.button !== 0)) return;
        const list = trackRef.current?.children?.[0];
        const rect = list?.getBoundingClientRect();
        dragRef.current = {
          id: event.pointerId,
          last: vertical ? event.clientY : event.clientX,
          period: rect ? (vertical ? rect.height : rect.width) : period
        };
        try { event.currentTarget.setPointerCapture(event.pointerId); } catch {}
        event.currentTarget.dataset.dragging = 'true';
      }}
      onPointerMove={event => {
        const drag = dragRef.current;
        if (!drag || drag.id !== event.pointerId) return;
        const position = vertical ? event.clientY : event.clientX;
        const delta = position - drag.last;
        drag.last = position;
        const length = Math.max(1, drag.period || period);
        setDragOffset(current => {
          let next = (current + delta) % length;
          if (next > length / 2) next -= length;
          if (next < -length / 2) next += length;
          return next;
        });
      }}
      onPointerUp={event => {
        if (dragRef.current?.id === event.pointerId) dragRef.current = null;
        delete event.currentTarget.dataset.dragging;
        try { event.currentTarget.releasePointerCapture(event.pointerId); } catch {}
      }}
      onPointerCancel={event => {
        if (dragRef.current?.id === event.pointerId) dragRef.current = null;
        delete event.currentTarget.dataset.dragging;
        try { event.currentTarget.releasePointerCapture(event.pointerId); } catch {}
      }}
    >
      <div className="logoloop__track" ref={trackRef}>
        {copies.map(copy => (
          <ul className="logoloop__list" key={copy} role="list" aria-hidden={copy === 1 ? 'true' : undefined}>
            {logos.map((item, index) => {
              const key = `${copy}-${index}`;
              const isNode = Object.prototype.hasOwnProperty.call(item, 'node');
              const visual = renderItem ? renderItem(item, key) : isNode
                ? <span className="logoloop__node" aria-hidden={!!item.href && !item.ariaLabel}>{item.node}</span>
                : <img src={item.src} srcSet={item.srcSet} sizes={item.sizes} width={item.width} height={item.height} alt={item.alt ?? ''} title={item.title} loading="lazy" decoding="async" draggable="false" />;
              const label = (labels === 'inline' || labels === 'below') && item.title
                ? <span className="logoloop__label" aria-hidden={!isNode && !!item.alt}>{item.title}</span>
                : null;
              const inner = <>{visual}{label}</>;
              const accessibleName = isNode ? (item.ariaLabel ?? item.title) : (item.alt ?? item.title);
              const itemStyle = item.hoverColor ? { '--logoloop-item-color': item.hoverColor } : undefined;
              const angle = logos.length > 1 ? (index / logos.length) * Math.PI * 2 : 0;
              const offset = layout === 'wave' ? Math.sin(angle) * clamp(bend, -1, 1) * 18 : layout === 'arc' ? -Math.sin(Math.PI * index / Math.max(1, logos.length - 1)) * Math.abs(bend) * 18 : 0;
              return (
                <li
                  className="logoloop__item"
                  key={key}
                  style={{ ...itemStyle, '--logoloop-index': index, '--logoloop-offset': `${offset}px` }}
                  data-color={item.hoverColor ? '' : undefined}
                  role="listitem"
                >
                  {item.href ? (
                    <a className="logoloop__content" href={item.href} aria-label={accessibleName || 'Logo link'} target="_blank" rel="noreferrer noopener" draggable="false" tabIndex={copy === 1 ? -1 : undefined}>
                      {inner}
                    </a>
                  ) : <div className="logoloop__content">{inner}</div>}
                  {labels === 'hover' && item.title && <span className="logoloop__tip" aria-hidden="true">{item.title}</span>}
                </li>
              );
            })}
          </ul>
        ))}
      </div>
    </div>
  );
});

export { LogoLoop };
export default LogoLoop;
