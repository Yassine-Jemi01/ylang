# JSON في YLang

توفر مساحة الأسماء `json` محللاً ومجموعة دوال للوصول إلى القيم وتنسيق المستندات. الواجهة جزء من C Runtime ولا تحتاج اعتماداً خارجياً.

## قراءة مستند

```ylang
function main() -> int {
    let owned_string document = json.parse("{\"name\":\"YLang\",\"version\":23}");
    print(json.get_string(document, "name"));
    print(json.get_int(document, "version"));
    return 0;
}
```

`json.parse(text)` يتحقق من JSON ويحتفظ بنسخة منه. استخدم `json.valid(text)` للتحقق من الصياغة دون الاحتفاظ بنسخة. تُرجع `json.get` قيمة الخاصية كنص JSON خام، بينما تفك `json.get_string` النص وتحوّل `json.get_int` و`json.get_float` و`json.get_bool` القيم إلى أنواعها.

## الدوال

| الدالة | الغرض |
|---|---|
| `json.parse(text)` | التحقق من المستند والاحتفاظ به. |
| `json.valid(text)` | فحص الصياغة. |
| `json.stringify(raw)` | التحقق من JSON خام ونسخه. |
| `json.pretty(raw)` | تنسيق JSON بمسافتين لكل مستوى. |
| `json.get(document, key)` | جلب قيمة خام من Object. |
| `json.get_string` و`json.get_int` و`json.get_float` و`json.get_bool` | جلب قيمة وتحويلها إلى النوع المطلوب. |
| `json.has(document, key)` | التحقق من وجود المفتاح. |
| `json.array_length(array)` و`json.at(array, index)` | معرفة طول Array وقراءة عنصر. |
| `json.escape(text)` | ترميز نص كسلسلة JSON صالحة. |
| `json.last_error()` | قراءة آخر تشخيص JSON. |

إذا لم يوجد المفتاح، ترجع `json.get` و`json.at` نصاً فارغاً. أما Getters ذات النوع المحدد فتعيد القيمة الصفرية وتضبط `json.last_error()`. استخدم `json.has` قبل الوصول إلى خاصية اختيارية.

## Builders غير القابلة للتعديل — تطوير 2.3

تضيف سلسلة 2.3 دوال بناء تعيد نص JSON جديداً، ولا تعدّل النص الأصلي:

- `json.object()` و`json.array()` لإنشاء حاوية فارغة.
- `json.set(object, key, raw_value)` لإضافة خاصية أو استبدالها.
- `json.remove(object, key)` لإزالة خاصية.
- `json.push(array, raw_value)` لإضافة عنصر.

يجب أن تكون القيم الممررة إلى `json.set` و`json.push` أجزاء JSON صالحة مسبقاً. استخدم `json.escape` لترميز النصوص.

```ylang
function main() -> int {
    let owned_string empty = json.object();
    let owned_string name = json.escape("Ada Lovelace");
    let owned_string profile = json.set(empty, "name", name);
    let owned_string complete = json.set(profile, "score", "42");
    print(complete);
    return 0;
}
```

كل Builder يعيد `owned_string` جديداً ويترك المدخل الأصلي كما هو. هذه واجهة Raw JSON وليست شجرة DOM أو كائنات قابلة للتعديل.

## المفاتيح المكررة وUnicode

تعيد Getters أول ظهور للمفتاح المكرر. `json.set` يستبدل أول ظهور ويحذف النسخ اللاحقة من المفتاح نفسه، بينما `json.remove` يحذف كل مرات ظهوره. تُحفظ بقية الخصائص وترتيبها الأصلي.

يتحقق `json.escape` ومفاتيح Builders من UTF-8. إذا كان النص غير صالح، تعيد الدالة نصاً فارغاً وتضع تشخيصاً في `json.last_error()`. لا تدعم YLang حالياً NUL داخل قيمة `string`؛ لذلك يُبلّغ عن U+0000 عند فك `json.get_string`.

## حدود الأمان

- أقصى حجم للمدخل أو الناتج: 16 MiB.
- أقصى عمق: 128 حاوية Object/Array؛ الحاوية رقم 129 مرفوضة.
- يُسمح بناتج حجمه 16 MiB بالضبط، ويُرفض ما يتجاوزه دون اقتطاع صامت.
- العمليات متزامنة؛ لا تنفذ الشبكة أو قراءة الملفات بنفسها.

أدوات Builders جزء من تطوير 2.3 وليست متاحة في تثبيت 2.2.0. راجع [مرجع JSON الإنجليزي](../reference/JSON.md) لكل التفاصيل.