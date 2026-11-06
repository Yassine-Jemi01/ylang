# أساسيات اللغة

هذا الفصل يقدم مجموعة من المفاهيم الأساسية. المرجع الإنجليزي للمواصفات هو المصدر الأدق عند الحاجة إلى تفاصيل قواعد اللغة.

## التصريحات والأنواع

تستخدم YLang تصريحات واضحة النوع:

```ylang
let int age = 16;
let float price = 12.5;
let bool enabled = true;
let string title = "Learning YLang";
```

اسم النوع يأتي قبل اسم المتغير. الأنواع الأساسية تشمل `int` و`float` و`bool` و`char` و`byte` و`string`.

لإنشاء قيمة ثابتة استخدم `let const`:

```ylang
let const int MAX_USERS = 100;
```

## الدوال

تبدأ الدالة بالكلمة `function`، وتحدد أنواع المعاملات ونوع القيمة المعادة:

```ylang
function add(int left, int right) -> int {
    return left + right;
}
```

استدعاء الدالة يكون باسمها مع القيم المطلوبة:

```ylang
let int total = add(20, 22);
```

## الشروط

```ylang
if (total == 42) {
    print("Correct");
} else {
    print("Try again");
}
```

## الحلقات

تدعم اللغة `while` و`for` و`loop()`. استخدم `break` للخروج من الحلقة و`continue` للانتقال إلى الدورة التالية.

```ylang
let int count = 0;
while (count < 3) {
    print(count);
    count++;
}
```

## المصفوفات

المصفوفات ثابتة الحجم وتحتوي عناصر من النوع نفسه:

```ylang
let int scores[] = [10, 20, 30];
print(scores[0]);
```

الوصول خارج حدود المصفوفة يؤدي إلى خطأ وقت التشغيل. لا تدعم النسخة الحالية إرجاع المصفوفات من الدوال أو المصفوفات المتداخلة.

## تابع التعلّم

انتقل إلى [YLanglings](ylanglings.md) لتطبيق هذه المفاهيم. راجع دائماً [مواصفات اللغة الإنجليزية](../reference/language-spec.md) لمعرفة القيود الدقيقة.
