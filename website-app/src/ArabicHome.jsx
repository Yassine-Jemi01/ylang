import { useState } from 'react';
import {
  FiArrowLeft,
  FiArrowUpRight,
  FiBookOpen,
  FiCheck,
  FiCode,
  FiGithub,
  FiLayers,
  FiMenu,
  FiTerminal,
  FiX
} from 'react-icons/fi';

const REPO = 'https://github.com/Yassine-Jemi01/ylang';

function Header() {
  const [menuOpen, setMenuOpen] = useState(false);

  return (
    <header className="site-header">
      <div className="container nav-wrap">
        <a className="brand" href="./" aria-label="الصفحة الرئيسية لـ YLang">
          <img className="brand-wordmark" src="../ylang-logo-dark.svg" alt="YLang" width="112" height="37" />
          <span className="brand-version">2.3.0</span>
        </a>

        <button
          className="mobile-menu"
          type="button"
          aria-label={menuOpen ? 'إغلاق القائمة' : 'فتح القائمة'}
          aria-expanded={menuOpen}
          onClick={() => setMenuOpen(value => !value)}
        >
          {menuOpen ? <FiX /> : <FiMenu />}
        </button>

        <nav className={menuOpen ? 'site-nav site-nav--open' : 'site-nav'} aria-label="التنقل الرئيسي">
          <a href="#features" onClick={() => setMenuOpen(false)}>اللغة</a>
          <a href="#release" onClick={() => setMenuOpen(false)}>إصدار 2.3</a>
          <a href="#install" onClick={() => setMenuOpen(false)}>البدء</a>
          <a href="../docs-ar.html" onClick={() => setMenuOpen(false)}>الوثائق</a>
        </nav>

        <details className="language-switcher">
          <summary aria-label="اختيار لغة الموقع">AR <span aria-hidden="true">⌄</span></summary>
          <div className="language-menu">
            <a href="../index.html" lang="en" hreflang="en">English (EN)</a>
            <a href="./" lang="ar" hreflang="ar" aria-current="page">العربية (AR)</a>
          </div>
        </details>

        <a className="nav-github" href={REPO} target="_blank" rel="noreferrer">
          <FiGithub aria-hidden="true" /> <span>GitHub</span><FiArrowUpRight className="nav-external" aria-hidden="true" />
        </a>
      </div>
    </header>
  );
}

function HeroPreview() {
  const source = [
    'function main() -> int {',
    '    let int answer = 40 + 2;',
    '    print("Hello, YLang");',
    '    print(answer);',
    '    return 0;',
    '}'
  ].join('\n');

  return (
    <div className="hero-preview-wrap" aria-label="مثال على كود YLang ومخرجات المترجم">
      <div className="hero-preview-glow" aria-hidden="true" />
      <section className="hero-preview" dir="ltr">
        <div className="preview-window-bar">
          <div className="preview-window-title">
            <span className="preview-window-dots" aria-hidden="true"><i /><i /><i /></span>
            <span>hello.yl</span>
          </div>
          <span className="preview-window-tag">YLANG SOURCE</span>
        </div>
        <pre className="preview-code"><code>{source}</code></pre>
        <div className="preview-terminal">
          <div className="preview-terminal-heading"><span><i /> TERMINAL</span><span>EXAMPLE OUTPUT</span></div>
          <pre><code><span className="preview-prompt">$</span> ylang build hello.yl -o hello{'\n'}<span className="preview-success">✓</span> Compiled to native executable{'\n'}<span className="preview-prompt">$</span> ./hello{'\n'}Hello, YLang{'\n'}42</code></pre>
        </div>
        <div className="preview-window-foot"><span><i /> C BACKEND</span><span>GCC / CLANG</span><span>NATIVE OUTPUT</span></div>
      </section>
    </div>
  );
}

function FeatureCard({ number, icon, title, description, meta }) {
  return (
    <article className="feature-card">
      <div className="feature-card-top">
        <span className="feature-number">{number}</span>
        <span className="feature-icon">{icon}</span>
      </div>
      <h3>{title}</h3>
      <p>{description}</p>
      <span className="feature-meta" dir="ltr">{meta}</span>
    </article>
  );
}

function ArabicInstallPanel() {
  return (
    <div className="install-panel">
      <div className="install-panel-heading">
        <span className="panel-step">01</span>
        <div><h3>تثبيت أدوات البناء</h3><p>اختر نظام التطوير لديك.</p></div>
      </div>
      <div className="arabic-install-commands" dir="ltr">
        <div><span>Fedora</span><pre><code>sudo dnf install gcc make clang</code></pre></div>
        <div><span>Ubuntu</span><pre><code>sudo apt install build-essential clang make</code></pre></div>
        <div><span>macOS</span><pre><code>xcode-select --install</code></pre></div>
      </div>
      <div className="install-divider" />
      <div className="install-panel-heading">
        <span className="panel-step">02</span>
        <div><h3>ابنِ المترجم</h3><p>المصدر والاختبارات وأدلة اللغة متاحة في المستودع.</p></div>
      </div>
      <div className="arabic-build-command" dir="ltr">
        <pre><code>make</code></pre>
        <pre><code>./build/ylang check examples/hello.yl</code></pre>
      </div>
      <p className="install-result"><FiCheck aria-hidden="true" /> اتبع دليل البدء للحصول على الخطوات الكاملة.</p>
    </div>
  );
}

function ArabicHome() {
  return (
    <>
      <a className="skip-link" href="#main">انتقل إلى المحتوى</a>
      <Header />
      <main id="main">
        <section className="hero" id="top">
          <div className="container hero-grid">
            <div className="hero-copy">
              <p className="eyebrow"><span className="eyebrow-mark" /> لغة مترجمة ومفتوحة المصدر</p>
              <div className="release-pill"><span className="release-dot" /> إصدار مستقر <span className="pill-divider" /> v2.3.0</div>
              <h1>اكتب بوضوح.<br /><span>وابنِ برامج أصلية.</span></h1>
              <p className="hero-lede">YLang لغة برمجة مترجمة بأنواع صريحة ورسائل تشخيص مفيدة، مع مترجم يمكنك استكشاف طريقة عمله. اكتب كودًا واضحًا، ثم ابنِ ملفًا تنفيذيًا أصليًا.</p>
              <div className="hero-actions">
                <a className="button button-primary" href="../learn/ar/start-here.html">ابدأ التعلّم <FiArrowLeft aria-hidden="true" /></a>
                <a className="button button-secondary" href="../docs-ar.html">الوثائق <FiArrowUpRight aria-hidden="true" /></a>
                <a className="button button-secondary" href={REPO} target="_blank" rel="noreferrer">المصدر <FiGithub aria-hidden="true" /></a>
              </div>
              <p className="hero-footnote"><FiCode aria-hidden="true" /> نواة صغيرة. مترجم حقيقي. مشروع مفتوح المصدر.</p>
              <a className="hero-dev-link" href="../learn/reference/CHANGELOG-2.3.0.html">
                <span className="hero-dev-dot" aria-hidden="true" />
                ملاحظات إصدار YLang 2.3.0
                <FiArrowUpRight aria-hidden="true" />
              </a>
            </div>
            <HeroPreview />
          </div>
          <div className="hero-bottom-line" aria-hidden="true" />
        </section>

        <section className="ecosystem" aria-label="المنصات وأدوات البناء">
          <div className="container ecosystem-inner">
            <p className="ecosystem-label">سلسلة أدوات تركّز على المترجم</p>
            <div className="arabic-platforms">
              <span>C17</span><span>Linux</span><span>macOS</span><span>Windows</span><span>GCC / Clang</span>
            </div>
          </div>
        </section>

        <section className="section language-section" id="features">
          <div className="container">
            <div className="section-heading">
              <p className="eyebrow"><span className="eyebrow-mark" /> اللغة</p>
              <h2>مصممة لتكون مفهومة.</h2>
              <p>تحافظ YLang على نواة صغيرة وواضحة، حتى يسهل قراءة البرامج وتتبع الأخطاء.</p>
            </div>
            <div className="feature-grid">
              <FeatureCard number="01" icon={<FiCode />} title="أنواع صريحة" description="تعلن نوع القيمة بوضوح، ويفحص المترجم الأنواع والتهيئة قبل بناء البرنامج." meta="EXPLICIT TYPES" />
              <FeatureCard number="02" icon={<FiCheck />} title="تشخيصات مفيدة" description="تكشف الأخطاء الشائعة مبكرًا وتوجّهك إلى موضع المشكلة في المصدر." meta="DIAGNOSTICS" />
              <FeatureCard number="03" icon={<FiTerminal />} title="برامج أصلية" description="يولّد المسار الافتراضي شيفرة C ويستخدم GCC أو Clang لإنتاج ملف تنفيذي." meta="C BACKEND" />
            </div>
          </div>
        </section>

        <section className="section preview-section" id="release">
          <div className="container">
            <div className="section-heading">
              <p className="eyebrow"><span className="eyebrow-mark" /> 2.3 / مستقر</p>
              <h2>ما الذي يقدمه الإصدار 2.3.0؟</h2>
              <p>يضم الإصدار ميزات موثقة، مع توضيح الحدود التجريبية لكل امتداد قبل استخدامه.</p>
            </div>
            <div className="feature-grid preview-grid arabic-release-grid">
              <FeatureCard number="01" icon={<FiLayers />} title="طلبات HTTP وواجهات API" description="إرسال الطلبات مع الترويسات والأجسام ورموز الحالة وحدود واضحة للاستجابات." meta="EXPERIMENTAL · LIBCURL" />
              <FeatureCard number="02" icon={<FiCode />} title="مهام HTTP غير المتزامنة" description="وضع الطلبات في قائمة انتظار ثم انتظار الاستجابة أو إلغاء المهمة." meta="ASYNC HTTP" />
              <FeatureCard number="03" icon={<FiTerminal />} title="تصحيح الأخطاء" description="استخدام رموز التصحيح وربط أسطر المصدر مع GDB أو LLDB." meta="GDB · LLDB" />
            </div>
            <div className="preview-links">
              <a className="text-link" href="../learn/reference/HTTP.html">دليل HTTP والمهام غير المتزامنة <FiArrowUpRight aria-hidden="true" /></a>
              <a className="text-link" href="../learn/reference/JSON.html">دليل JSON <FiArrowUpRight aria-hidden="true" /></a>
              <a className="text-link" href="../learn/reference/DEBUGGER.html">دليل المصحح <FiArrowUpRight aria-hidden="true" /></a>
            </div>
          </div>
        </section>

        <section className="section install-section" id="install">
          <div className="container install-grid">
            <div className="install-copy">
              <p className="eyebrow"><span className="eyebrow-mark" /> ابدأ هنا</p>
              <h2>من المصدر إلى<br /><span>أول عملية بناء.</span></h2>
              <p>ابنِ المترجم، افحص برنامجًا، ثم أنشئ ملفًا تنفيذيًا. يتضمن المستودع المترجم والاختبارات ووثائق اللغة.</p>
              <a className="text-link" href="../learn/ar/start-here.html">اتبع دليل البدء <FiArrowUpRight aria-hidden="true" /></a>
              <div className="install-note"><span className="note-mark">i</span><p>المسار الافتراضي يستخدم C. ما يزال NASM وواجهات GUI وGame2D والمسار الأصلي لـ Windows تجريبية.</p></div>
            </div>
            <ArabicInstallPanel />
          </div>
        </section>

        <section className="section next-section">
          <div className="container">
            <div className="next-intro">
              <div><p className="eyebrow"><span className="eyebrow-mark" /> تابع الاستكشاف</p><h2>اختر نقطة البداية.</h2></div>
              <p>كل الموارد الأساسية متاحة من هنا؛ لا تحتاج إلى المرور ببوابة الوثائق للوصول إلى الصفحة الرئيسية العربية.</p>
            </div>
            <div className="next-grid">
              <a className="next-card" href="../learn/ar/start-here.html">
                <span className="next-icon"><FiBookOpen /></span><h3>كتاب YLang</h3><p>مسار تعليمي عربي يبدأ من الصفر.</p><span className="next-link">ابدأ القراءة <FiArrowUpRight /></span>
              </a>
              <a className="next-card" href="../docs-ar.html">
                <span className="next-icon"><FiLayers /></span><h3>الوثائق العربية</h3><p>الأدلة والمراجع التقنية وملاحظات الإصدار.</p><span className="next-link">افتح الوثائق <FiArrowUpRight /></span>
              </a>
              <a className="next-card" href={REPO} target="_blank" rel="noreferrer">
                <span className="next-icon"><FiGithub /></span><h3>المستودع البرمجي</h3><p>المصدر والاختبارات والمساهمة في المشروع.</p><span className="next-link">GitHub <FiArrowUpRight /></span>
              </a>
            </div>
          </div>
        </section>
      </main>
      <footer className="site-footer">
        <div className="container footer-main">
          <a className="brand footer-brand" href="./"><img className="brand-mark" src="../ylang-mark.svg" alt="" width="28" height="28" />YLang</a>
          <p>لغة صغيرة. مترجم حقيقي. مفتوحة المصدر.</p>
          <nav className="footer-links" aria-label="روابط إضافية">
            <a href="../docs-ar.html">الوثائق</a>
            <a href="../learn/ar/start-here.html">الكتاب</a>
            <a href={REPO} target="_blank" rel="noreferrer">GitHub <FiArrowUpRight /></a>
          </nav>
        </div>
        <div className="container footer-bottom"><span>YLang 2.3.0</span><span>المصدر المفتوح، بوضوح.</span></div>
      </footer>
    </>
  );
}

export default ArabicHome;
