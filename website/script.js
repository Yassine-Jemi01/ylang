/* YLang site behaviour: language switch (EN/AR), mobile menu, copy buttons, diagram reveal.
 * Plain script, no build step. */
(() => {
  "use strict";

  /* Translations. English text lives in index.html and is read from the page at
   * startup, so it is never duplicated here. To add a language: add an entry to
   * `languages` and a block to `strings`. Keys match the `data-i18n` /
   * `data-i18n-attr` attributes in index.html. */
  const I18N = {
    languages: {
      en: { dir: "ltr", switchLabel: "Switch to English", buttonText: "English" },
      ar: { dir: "rtl", switchLabel: "التبديل إلى العربية", buttonText: "العربية" }
    },

    /* English strings that have no matching element in the HTML. */
    extra: {
      "menu.open": "Open navigation",
      "menu.close": "Close navigation",
      "copy.done": "Copied!",
      "copy.fail": "Select text to copy"
    },

    strings: {
      ar: {
        "skip": "انتقل إلى المحتوى",
        "meta.title": "YLang — اكتب بوضوح. ترجِم بتأنٍّ.",
        "meta.desc": "YLang لغة برمجة مفتوحة المصدر تُترجَم إلى ملفات تنفيذية، بصياغة مقروءة وأنواع صريحة.",
        "nav.label": "التنقل الرئيسي",
        "menu.open": "فتح القائمة",
        "menu.close": "إغلاق القائمة",
        "nav.language": "اللغة",
        "nav.pipeline": "آلية البناء",
        "nav.learn": "التعلّم",
        "nav.start": "ابدأ الآن",
    
        "hero.title1": "اكتب بوضوح.",
        "hero.title2": "ترجِم بتأنٍّ.",
        "hero.lede": "YLang لغة برمجة مفتوحة المصدر تُترجَم إلى ملفات تنفيذية، مبنية على صياغة مقروءة وأنواع صريحة وسلسلة أدوات يمكنك فحصها والتعلّم منها والمساهمة في تحسينها.",
        "hero.cta1": "ابدأ التعلّم",
        "hero.cta2": "استكشف الشيفرة المصدرية",
        "facts.release": "الإصدار الحالي",
        "facts.stable": "الواجهة الخلفية المستقرة",
        "facts.stable.v": "C عبر GCC أو Clang",
        "facts.exp": "تجريبي",
        "facts.exp.v": "NASM على Linux x86-64",
        "code.label": "مثال شيفرة بلغة YLang",
        "copy.code": "نسخ الشيفرة",
        "copy.cmds": "نسخ الأوامر",
        "copy.done": "تم النسخ",
        "copy.fail": "حدّد النص لنسخه يدويًا",
    
        "lang.title": "مألوفة في القراءة، مصمَّمة للاستكشاف.",
        "lang.lede": "تقدّم YLang صياغة سهلة الاقتراب ضمن مشروع لغة مُترجَمة صغير. وتنفيذها مفتوح، فتصبح قرارات التصميم جزءًا من تجربة التعلّم أيضًا.",
        "lang.f1.t": "شيفرة مقروءة",
        "lang.f1.d": "بنية التحكم المألوفة وتواقيع الدوال الواضحة تُبقي البرامج سهلة الفهم دون إخفاء طريقة عملها.",
        "lang.f2.t": "أنواع صريحة",
        "lang.f2.d": "الأنواع والتحويلات ظاهرة في الشيفرة، مع رسائل تشخيص تساعدك على فهم الأخطاء.",
        "lang.f3.t": "مترجِم يمكنك فحصه",
        "lang.f3.d": "المسار المستقر يولّد شيفرة C ويستعين بـ GCC أو Clang، أما الواجهة الخلفية NASM فما تزال تجريبية ومحدودة.",
    
        "pipe.title": "من ملف .yl إلى برنامج أصلي",
        "pipe.lede": "للمترجِم مساران: مسار C وهو المستقر، ومسار NASM وهو تجريبي ويغطي جزءًا أقل من اللغة.",
        "pipe.caption": "مخطط البناء: ينتقل ملف المصدر إلى مترجِم YLang، وله واجهة خلفية مستقرة بلغة C تُبنى عبر GCC أو Clang، وواجهة خلفية تجريبية هي NASM.",
        "pipe.source": "شيفرتك المصدرية",
        "pipe.compiler": "مترجِم YLang",
        "pipe.compiler.d": "يقرأ شيفرتك ويفحصها",
        "pipe.c": "الواجهة الخلفية C",
        "pipe.c.d": "يولّد شيفرة C",
        "pipe.cc.d": "يبني شيفرة C المولَّدة",
        "pipe.exe": "ملف تنفيذي أصلي",
        "pipe.exe.d": "يعمل بمفرده",
        "pipe.nasm": "الواجهة الخلفية NASM",
        "pipe.nasm.d": "على Linux x86-64 فقط، بتغطية محدودة",
        "tag.stable": "مستقر",
        "tag.exp": "تجريبي",
        "tag.none": "غير متاح بعد",
        "tag.evolving": "قيد التطوّر",
    
        "status.title": "ما الذي يعمل اليوم",
        "status.c": "الواجهة الخلفية C",
        "status.c.d": "تولّد شيفرة C وتُبنى عبر GCC أو Clang.",
        "status.nasm": "الواجهة الخلفية NASM",
        "status.nasm.d": "على Linux x86-64 فقط، مع تغطية محدودة لميزات اللغة.",
        "status.inst": "مثبّتات جاهزة",
        "status.inst.d": "لم يُنشر أي منها. ابنِ المشروع من المصدر.",
        "status.safe": "نموذج الأمان",
        "status.safe.d": "لا يعادل نموذج Rust. ويختلف الدعم باختلاف الواجهة الخلفية.",
    
        "learn.title": "ابدأ بالدليل أو بالشيفرة.",
        "learn.lede": "اقرأ الكتاب، أو تصفّح التوثيق، أو ابنِ المترجِم على جهازك. يمكنك متابعة المشروع من مصدره بدل الاعتماد على صندوق مغلق.",
        "learn.link": "تصفّح التوثيق",
        "res.book.t": "كتاب YLang",
        "res.book.d": "اقرأ كتاب اللغة المتوفر بصيغة PDF.",
        "res.docs.t": "توثيق اللغة",
        "res.docs.d": "البنية، ومواصفات اللغة، وملاحظات المترجِم.",
        "res.ex.t": "برامج أمثلة",
        "res.ex.d": "جرّب برامج صغيرة وتعلّم بتعديلها.",
    
        "start.title": "ابنِها من المصدر.",
        "start.lede": "لم تُنشر مثبّتات جاهزة بعد. لتجربة YLang اليوم، انسخ المستودع وابنِ المترجِم الحالي.",
        "start.releases": "عرض الإصدارات",
        "start.build": "تعليمات البناء",
        "start.req": "يتطلب مترجِم C17 وأداة GNU Make. راجع ملف README لملاحظات المنصات.",
    
        "comm.title": "ساهم في رسم ما يأتي بعد ذلك.",
        "comm.lede": "اقرأ التنفيذ، أو أبلِغ عن خطأ، أو ساهم في تطوير اللغة.",
        "comm.cta": "زر YLang على GitHub",
    
        "foot.desc": "مشروع لغة برمجة مفتوح المصدر، صُنع ليُستكشف.",
        "foot.license": "رخصة MIT",
        "foot.book": "الكتاب",
        "foot.contrib": "مساهمو YLang",
        "foot.tag": "صُمّم للوضوح."
      }
    }
  };

  const { languages, strings, extra } = I18N;
  const root = document.documentElement;
  const $ = (selector, scope = document) => scope.querySelector(selector);
  const $$ = (selector, scope = document) => [...scope.querySelectorAll(selector)];

  /* localStorage can throw (private mode, blocked cookies): never let it break the page. */
  const storage = {
    get(key) { try { return localStorage.getItem(key); } catch { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch { /* ignore */ } }
  };

  /* ---------- Language ---------- */

  const metaDescription = $('meta[name="description"]');
  const langButton = $("#lang-toggle");
  const attrTargets = $$("[data-i18n-attr]").map((el) => {
    const [attr, key] = el.dataset.i18nAttr.split(":");
    return { el, attr, key };
  });

  /* English is read from the page itself, then Arabic is layered on top. */
  const dict = { en: { ...extra }, ...strings };
  $$("[data-i18n]").forEach((el) => { dict.en[el.dataset.i18n] = el.textContent; });
  attrTargets.forEach(({ el, attr, key }) => { dict.en[key] = el.getAttribute(attr); });
  dict.en["meta.title"] = document.title;
  dict.en["meta.desc"] = metaDescription ? metaDescription.content : "";

  let lang = languages[root.lang] ? root.lang : "en";
  const t = (key) => dict[lang]?.[key] ?? dict.en[key] ?? "";

  function applyLanguage(next) {
    lang = languages[next] ? next : "en";
    root.lang = lang;
    root.dir = languages[lang].dir;

    $$("[data-i18n]").forEach((el) => { el.textContent = t(el.dataset.i18n); });
    attrTargets.forEach(({ el, attr, key }) => el.setAttribute(attr, t(key)));
    document.title = t("meta.title");
    if (metaDescription) metaDescription.content = t("meta.desc");

    /* The button names the language you can switch to. */
    const other = lang === "ar" ? "en" : "ar";
    if (langButton) {
      langButton.textContent = languages[other].buttonText;
      langButton.lang = other;
      langButton.setAttribute("aria-label", languages[other].switchLabel);
    }
    updateMenuLabel();
  }

  langButton?.addEventListener("click", () => {
    const next = lang === "ar" ? "en" : "ar";
    applyLanguage(next);
    storage.set("ylang-lang", next);
  });

  /* ---------- Mobile menu ---------- */

  const menuButton = $(".menu-toggle");
  const nav = $("#site-nav");

  function updateMenuLabel() {
    if (!menuButton) return;
    const open = menuButton.getAttribute("aria-expanded") === "true";
    menuButton.setAttribute("aria-label", t(open ? "menu.close" : "menu.open"));
  }

  function setMenu(open) {
    menuButton.setAttribute("aria-expanded", String(open));
    nav.classList.toggle("is-open", open);
    updateMenuLabel();
  }

  if (menuButton && nav) {
    menuButton.addEventListener("click", () => setMenu(menuButton.getAttribute("aria-expanded") !== "true"));
    nav.addEventListener("click", (event) => { if (event.target.closest("a")) setMenu(false); });
    document.addEventListener("keydown", (event) => { if (event.key === "Escape") setMenu(false); });
    window.matchMedia("(min-width: 821px)").addEventListener("change", (event) => { if (event.matches) setMenu(false); });
  }

  /* ---------- Copy buttons ---------- */

  $$(".copy-button").forEach((button) => {
    let timer;
    button.addEventListener("click", async () => {
      const source = button.dataset.copy ? document.getElementById(button.dataset.copy) : null;
      const text = button.dataset.copyText ?? source?.innerText ?? "";
      if (!text) return;

      let key = "copy.done";
      try { await navigator.clipboard.writeText(text); } catch { key = "copy.fail"; }

      button.textContent = t(key);
      clearTimeout(timer);
      timer = setTimeout(() => { button.textContent = t(button.dataset.i18n); }, 1800);
    });
  });

  /* ---------- Build diagram: light the steps up in order, once ---------- */

  const flow = $(".flow");
  if (flow) {
    /* Document order is the left-to-right (or right-to-left) reading order. */
    $$(".node, .link, .branches", flow).forEach((el, index) => el.style.setProperty("--i", index));

    const reveal = () => flow.classList.add("is-in");
    if ("IntersectionObserver" in window) {
      const observer = new IntersectionObserver((entries) => {
        if (entries.some((entry) => entry.isIntersecting)) { reveal(); observer.disconnect(); }
      }, { threshold: 0.25 });
      observer.observe(flow);
    } else {
      reveal();
    }
  }

  /* ---------- Footer year & first render ---------- */

  const year = $("#year");
  if (year) year.textContent = String(new Date().getFullYear());

  applyLanguage(lang);
})();
