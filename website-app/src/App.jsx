import { useState } from 'react';
import { DropdownMenu, Tabs } from 'radix-ui';
import {
  FiArrowRight,
  FiArrowUpRight,
  FiBookOpen,
  FiCheck,
  FiChevronDown,
  FiCode,
  FiCpu,
  FiCopy,
  FiDownload,
  FiGithub,
  FiLayers,
  FiMonitor,
  FiMenu,
  FiTerminal,
  FiX
} from 'react-icons/fi';
import { SiApple, SiC, SiLinux } from 'react-icons/si';
import LogoLoop from './components/LogoLoop.jsx';
import TextLoop from './components/TextLoop.jsx';

const REPO = 'https://github.com/Yassine-Jemi01/ylang';
const SITE = 'https://yassine-jemi01.github.io/ylang/';
const BOOK = `${SITE}learn/index.html`;
const DOCS = `${SITE}docs.html`;
const PDF = `${SITE}YLang-Book.pdf`;

const installCommands = {
  fedora: 'sudo dnf install gcc make clang',
  ubuntu: 'sudo apt install build-essential clang make',
  macos: 'xcode-select --install\nbrew install make llvm'
};

const ecosystem = [
  { node: <SiC />, title: 'C17 compiler', hoverColor: '#7cc6bb' },
  { node: <SiLinux />, title: 'Linux', hoverColor: '#f2bf61' },
  { node: <SiApple />, title: 'macOS', hoverColor: '#e6eeeb' },
  { node: <FiMonitor />, title: 'Windows · experimental', hoverColor: '#77aee5' },
  { node: <FiCpu />, title: 'GCC / Clang', hoverColor: '#9ecf9a' },
  { node: <FiTerminal />, title: 'Clang', hoverColor: '#f3a77d' },
  { node: <FiGithub />, title: 'Open source', href: REPO, hoverColor: '#e7ece9' }
];

function Mark() {
  return <span className="brand-mark" aria-hidden="true">Y</span>;
}

function CopyButton({ value, label = 'Copy', className = '' }) {
  const [copied, setCopied] = useState(false);
  async function copy() {
    try {
      await navigator.clipboard.writeText(value);
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1600);
    } catch {
      const field = document.createElement('textarea');
      field.value = value;
      field.setAttribute('readonly', '');
      field.style.position = 'fixed';
      field.style.opacity = '0';
      document.body.appendChild(field);
      field.select();
      const ok = document.execCommand('copy');
      field.remove();
      if (ok) {
        setCopied(true);
        window.setTimeout(() => setCopied(false), 1600);
      }
    }
  }
  return (
    <button className={`copy-button ${className}`.trim()} type="button" onClick={copy} aria-label={copied ? 'Copied' : label}>
      {copied ? <FiCheck aria-hidden="true" /> : <FiCopy aria-hidden="true" />}
      <span>{copied ? 'Copied' : label}</span>
    </button>
  );
}

function Header() {
  const [menuOpen, setMenuOpen] = useState(false);
  return (
    <header className="site-header">
      <div className="container nav-wrap">
        <a className="brand" href="#top" aria-label="YLang home">
          <Mark />
          <span>YLang</span>
          <span className="brand-version">2.0</span>
        </a>
        <button className="mobile-menu" type="button" aria-label={menuOpen ? 'Close navigation' : 'Open navigation'} aria-expanded={menuOpen} onClick={() => setMenuOpen(v => !v)}>
          {menuOpen ? <FiX /> : <FiMenu />}
        </button>
        <nav className={`site-nav ${menuOpen ? 'site-nav--open' : ''}`} aria-label="Main navigation">
          <a href="#language" onClick={() => setMenuOpen(false)}>Language</a>
          <a href="#install" onClick={() => setMenuOpen(false)}>Get started</a>
          <DropdownMenu.Root>
            <DropdownMenu.Trigger asChild>
              <button className="nav-dropdown-trigger" type="button">
                Resources <FiChevronDown aria-hidden="true" />
              </button>
            </DropdownMenu.Trigger>
            <DropdownMenu.Portal>
              <DropdownMenu.Content className="resource-menu" align="end" sideOffset={10}>
                <DropdownMenu.Item className="resource-menu-item" asChild>
                  <a href={BOOK}><FiBookOpen /> The YLang Book <FiArrowUpRight className="menu-external" /></a>
                </DropdownMenu.Item>
                <DropdownMenu.Item className="resource-menu-item" asChild>
                  <a href={DOCS}><FiLayers /> Reference & docs <FiArrowUpRight className="menu-external" /></a>
                </DropdownMenu.Item>
                <DropdownMenu.Item className="resource-menu-item" asChild>
                  <a href={PDF}><FiDownload /> Book PDF <FiArrowUpRight className="menu-external" /></a>
                </DropdownMenu.Item>
              </DropdownMenu.Content>
            </DropdownMenu.Portal>
          </DropdownMenu.Root>
        </nav>
        <a className="nav-github" href={REPO} target="_blank" rel="noreferrer">
          <FiGithub aria-hidden="true" /> <span>GitHub</span><FiArrowUpRight className="nav-external" aria-hidden="true" />
        </a>
      </div>
    </header>
  );
}

function Hero() {
  return (
    <section className="hero" id="top">
      <div className="container hero-grid">
        <div className="hero-copy">
          <p className="eyebrow"><span className="eyebrow-mark" /> OPEN SOURCE · BUILT IN THE OPEN</p>
          <div className="release-pill"><span className="release-dot" /> Stable release <span className="pill-divider" /> v2.0.0</div>
          <h1>Readable code.<br /><span>Native programs.</span></h1>
          <p className="hero-lede">A compiled programming language with explicit types, helpful diagnostics, and a compiler you can explore. Clear syntax in your editor; a native executable at the end.</p>
          <div className="hero-actions">
            <a className="button button-primary" href="#install">Get started <FiArrowRight aria-hidden="true" /></a>
            <a className="button button-secondary" href={BOOK}>Read the book <FiBookOpen aria-hidden="true" /></a>
          </div>
          <p className="hero-footnote"><FiCode aria-hidden="true" /> Small language. Real compiler. Open source.</p>
        </div>
      </div>
      <div className="hero-bottom-line" aria-hidden="true" />
    </section>
  );
}

function Ecosystem() {
  return (
    <section className="ecosystem" aria-label="Platforms and toolchain">
      <div className="container ecosystem-inner">
        <p className="ecosystem-label">A compiler-focused toolchain</p>
        <div className="ecosystem-loop">
          <LogoLoop
            logos={ecosystem}
            speed={42}
            direction="left"
            logoHeight={27}
            gap={44}
            hoverSpeed={0}
            scaleOnHover
            fadeOut
            fadeOutWidth={0.09}
            labels="inline"
            muted={0.12}
            ariaLabel="YLang toolchain and platform ecosystem"
          />
        </div>
      </div>
    </section>
  );
}

function FeatureCard({ number, icon, title, description, meta }) {
  return (
    <article className="feature-card">
      <div className="feature-card-top"><span className="feature-number">{number}</span><span className="feature-icon">{icon}</span></div>
      <h3>{title}</h3>
      <p>{description}</p>
      <span className="feature-meta">{meta}</span>
    </article>
  );
}

function LanguageSection() {
  return (
    <section className="section language-section" id="language">
      <div className="container">
        <div className="section-heading">
          <p className="eyebrow"><span className="eyebrow-mark" /> THE LANGUAGE</p>
          <h2>Made to be understood.</h2>
          <p>YLang keeps the core small and explicit, so the code is easy to read and errors are easier to trace.</p>
        </div>
        <div className="feature-grid">
          <FeatureCard number="01" icon={<FiCode />} title="Explicit types" description="Declare what a value is. The compiler checks types and initialization before it builds a program." meta="READABLE BY DEFAULT" />
          <FeatureCard number="02" icon={<FiCheck />} title="Useful diagnostics" description="Catch common mistakes early with messages that point you toward the source of a problem." meta="FEEDBACK BEFORE BUILD" />
          <FeatureCard number="03" icon={<FiTerminal />} title="Compiled output" description="The default backend generates C and invokes GCC or Clang to build a native executable." meta="NO INTERPRETER LOOP" />
        </div>
        <div className="text-loop-band" aria-label="YLang principles">
          <TextLoop
            text="OPEN SOURCE · EXPLICIT TYPES · NATIVE BUILDS"
            shape="wave"
            speed={24}
            separator="—"
            curviness={25}
            fontSize={25}
            fontWeight={750}
            letterSpacing={1.8}
            ribbon={false}
            color="#93d4c8"
            edgeFade={0.65}
            pauseOnHover
            draggable={false}
            scrollBoost={0}
          />
        </div>
      </div>
    </section>
  );
}

function InstallSection() {
  return (
    <section className="section install-section" id="install">
      <div className="container install-grid">
        <div className="install-copy">
          <p className="eyebrow"><span className="eyebrow-mark" /> START HERE</p>
          <h2>From source to<br /><span>your first build.</span></h2>
          <p>Build the compiler, check a program, then produce a native executable. The repository includes the compiler, tests, and language documentation.</p>
          <a className="text-link" href={BOOK}>Follow the getting-started guide <FiArrowUpRight aria-hidden="true" /></a>
          <div className="install-note"><span className="note-mark">i</span><p>The default workflow uses the C backend. The NASM backend, GUI/Game2D APIs, and native Windows path are experimental.</p></div>
        </div>
        <div className="install-panel">
          <div className="install-panel-heading">
            <span className="panel-step">01</span>
            <div><h3>Install the build tools</h3><p>Choose your development environment.</p></div>
          </div>
          <Tabs.Root className="install-tabs" defaultValue="fedora">
            <Tabs.List className="install-tab-list" aria-label="Operating system">
              <Tabs.Trigger className="install-tab" value="fedora"><SiLinux aria-hidden="true" /> Fedora</Tabs.Trigger>
              <Tabs.Trigger className="install-tab" value="ubuntu">Ubuntu</Tabs.Trigger>
              <Tabs.Trigger className="install-tab" value="macos"><SiApple aria-hidden="true" /> macOS</Tabs.Trigger>
            </Tabs.List>
            {Object.entries(installCommands).map(([os, command]) => (
              <Tabs.Content className="install-tab-content" value={os} key={os}>
                <pre><code>{command}</code></pre>
                <CopyButton value={command} label="Copy command" />
              </Tabs.Content>
            ))}
          </Tabs.Root>
          <div className="install-divider" />
          <div className="install-panel-heading">
            <span className="panel-step">02</span>
            <div><h3>Build YLang</h3><p>From the repository root.</p></div>
          </div>
          <div className="shell-block">
            <CopyButton value={'git clone https://github.com/Yassine-Jemi01/ylang.git\ncd ylang\nmake\nmake test'} label="Copy steps" />
            <pre><code><span>git clone https://github.com/Yassine-Jemi01/ylang.git</span><span>cd ylang</span><span>make</span><span>make test</span></code></pre>
          </div>
          <p className="install-result"><FiCheck aria-hidden="true" /> Compiler output: <code>build/ylang</code></p>
        </div>
      </div>
    </section>
  );
}

function NextSteps() {
  const items = [
    { icon: <FiBookOpen />, title: 'The YLang Book', text: 'Tutorials, language reference, and examples.', href: BOOK, label: 'Start learning' },
    { icon: <FiLayers />, title: 'Documentation', text: 'Compiler, platform, library, and backend notes.', href: DOCS, label: 'Explore docs' },
    { icon: <FiGithub />, title: 'Source code', text: 'Read the compiler, tests, and active development.', href: REPO, label: 'View repository' }
  ];
  return (
    <section className="section next-section" id="community">
      <div className="container">
        <div className="next-intro">
          <div><p className="eyebrow"><span className="eyebrow-mark" /> KEEP GOING</p><h2>Pick up where you are.</h2></div>
          <p>Whether you're learning the syntax or looking through the compiler, the next step is open to you.</p>
        </div>
        <div className="next-grid">
          {items.map(item => <a className="next-card" key={item.title} href={item.href} target={item.href.startsWith('http') ? '_blank' : undefined} rel={item.href.startsWith('http') ? 'noreferrer' : undefined}>
            <span className="next-icon">{item.icon}</span><h3>{item.title}</h3><p>{item.text}</p><span className="next-link">{item.label} <FiArrowUpRight aria-hidden="true" /></span>
          </a>)}
        </div>
      </div>
    </section>
  );
}

function Footer() {
  return (
    <footer className="site-footer">
      <div className="container footer-main">
        <a className="brand footer-brand" href="#top"><Mark /><span>YLang</span></a>
        <p>Readable code. Native programs.</p>
        <div className="footer-links">
          <a href={BOOK}>Book</a><a href={DOCS}>Docs</a><a href={REPO} target="_blank" rel="noreferrer">GitHub <FiArrowUpRight aria-hidden="true" /></a>
        </div>
      </div>
      <div className="container footer-bottom"><span>Open-source programming language project.</span><span>Stable release: 2.0.0</span></div>
    </footer>
  );
}

export default function App() {
  return (
    <>
      <a className="skip-link" href="#main">Skip to content</a>
      <Header />
      <main id="main">
        <Hero />
        <Ecosystem />
        <LanguageSection />
        <InstallSection />
        <NextSteps />
      </main>
      <Footer />
    </>
  );
}
