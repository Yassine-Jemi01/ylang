# واجهة الطرفية TUI

توفر §std.tui§ أدوات لبناء واجهات تفاعلية داخل Terminal، مثل قراءة المفاتيح والتحكم بالمؤشر ومسح الشاشة ومعرفة أبعاد الطرفية.

## المتطلبات

تحتاج TUI إلى stdin وstdout تفاعليين ومتصلين بطرفية. إذا كان أحدهما معاد توجيهه إلى ملف أو Pipe، فقد يرفض §tui.init()§ التهيئة.

## الدوال الأساسية

- §tui.init()§ لتهيئة الطرفية وحفظ حالتها الأصلية.
- §tui.read_key()§ لانتظار مفتاح وإرجاع رمزه.
- §tui.poll_key()§ لقراءة مفتاح متاح دون انتظار، أو إرجاع §-1§.
- §tui.width()§ و§tui.height()§ لقراءة أبعاد الطرفية.
- §tui.clear()§ لمسح الشاشة.
- §tui.write(text)§ لكتابة نص.
- §tui.set_cursor(column, row)§ لنقل المؤشر إلى إحداثيات تبدأ من 1.
- §tui.hide_cursor()§ و§tui.show_cursor()§ للتحكم بالمؤشر.
- §tui.sleep_ms(milliseconds)§ للانتظار.
- §tui.last_error()§ و§tui.shutdown()§ لقراءة الخطأ واستعادة الطرفية.

## مثال

§§§ylang
function main() -> int {
    if (!tui.init()) {
        print(tui.last_error());
        return 1;
    }
    tui.clear();
    tui.write("Press a key to exit: ");
    let int key = tui.read_key();
    tui.write("\nKey received.\n");
    tui.shutdown();
    return 0;
}
§§§

## الاستعادة وحدود المنصة

يحفظ Runtime إعدادات الإدخال والإخراج ويعيدها عند §tui.shutdown()§ أو عند خروج البرنامج. على POSIX تُثبت معالجات إشارات لاستعادة الطرفية عند الإشارات المدعومة. وعلى Windows يحفظ Runtime وضعي Console input وoutput ويعيدهما.

دعم Windows الأصلي ما زال محدوداً وتجريبياً؛ نجاح بناء TUI لا يعني تطابق كل سلوك تفاعلي مع Linux وmacOS. اختبر المفاتيح والمؤشر وأوضاع Console على النظام المستهدف.