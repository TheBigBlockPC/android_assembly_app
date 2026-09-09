package dev.armstudio;

import android.app.*;
import android.content.*;
import android.content.res.Configuration;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.os.*;
import android.text.*;
import android.text.style.*;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.regex.*;

public class MainActivity extends Activity {
    final Handler handler = new Handler(Looper.getMainLooper());
    final ExecutorService lintWorker = Executors.newSingleThreadExecutor();
    SharedPreferences prefs;
    int bg, surface, raised, ink, muted, accent, border, error, tab = 0, editorSize = 14,
                                                                docsSize = 15;
    LinearLayout root, body, nav, editorPage, runPage, inspectPage, guidePage;
    TextView statusView, diagnostic, suggestion, projectTitle, registerView, memoryView, console,
        modeLabel;
    CodeEdit editor;
    Gutter gutter;
    ScrollView codeScroll;
    FrameView frame;
    EditText address;
    Button runButton, playButton;
    String project = "Hello world", source = "", lintError = "";
    boolean changing = false, dirty = true, running = false, destroyed = false;
    int generation = 0, memoryAddress = 0x10000;
    int key = 0, touchX = 0, touchY = 0;
    boolean keyDown = false, touchDown = false;
    TreeSet<Integer> breakpoints = new TreeSet<>();
    ArrayList<String[]> catalog = new ArrayList<>();
    String completion = "";
    int prefixStart = 0, prefixEnd = 0;
    final String[] examples = {"Hello world", "Color field", "Touch canvas", "Keyboard echo",
                               "Float & stack"};
    final Runnable tick = new Runnable() {
        public void run() {
            if (!running || destroyed)
                return;
            Native.run(12000, false);
            if (Native.state() != 0)
                running = false;
            refresh();
            if (running)
                handler.postDelayed(this, 16);
        }
    };
    final Runnable lintTask = () -> {
        final String text = editor.getText().toString();
        final int id = ++generation;
        lintWorker.execute(() -> {
            String result = Native.lint(text);
            handler.post(() -> {
                if (destroyed || generation != id || !editor.getText().toString().equals(text))
                    return;
                lintError = result;
                diagnostic.setText(result.isEmpty()
                                       ? "✓  Valid assembly   ·   Tap line numbers for breakpoints"
                                       : result);
                diagnostic.setTextColor(result.isEmpty() ? muted : error);
                highlight();
            });
        });
    };
    int dp(float n) {
        return (int)(getResources().getDisplayMetrics().density * n + .5f);
    }
    @Override
    public void onCreate(Bundle state) {
        if ((getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) !=
            Configuration.UI_MODE_NIGHT_YES)
            setTheme(android.R.style.Theme_Material_Light_NoActionBar);
        super.onCreate(state);
        prefs = getSharedPreferences("studio", 0);
        editorSize = prefs.getInt("editorSize", 14);
        docsSize = prefs.getInt("docsSize", 15);
        project = prefs.getString("project", "Hello world");
        source = prefs.getString("draft", Native.example(0));
        boolean dark = (getResources().getConfiguration().uiMode &
                        Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        bg = Color.parseColor(dark ? "#0C1416" : "#F3F6F4");
        surface = Color.parseColor(dark ? "#131F22" : "#FFFFFF");
        raised = Color.parseColor(dark ? "#1C2C2F" : "#E7EEEA");
        ink = Color.parseColor(dark ? "#E5F0EA" : "#172C26");
        muted = Color.parseColor(dark ? "#8AA29C" : "#59736A");
        accent = Color.parseColor(dark ? "#83E6C0" : "#087B59");
        border = Color.parseColor(dark ? "#2B3D3F" : "#D5E1DA");
        error = Color.parseColor(dark ? "#FF8F94" : "#B6243F");
        for (String row : Native.catalog().split("\n"))
            catalog.add(row.split("\\|", 5));
        getWindow().setStatusBarColor(bg);
        getWindow().setNavigationBarColor(bg);
        if (!dark)
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        root = col();
        root.setBackgroundColor(bg);
        root.setPadding(dp(16), 0, dp(16), 0);
        setContentView(root);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            android.graphics.Insets bars =
                insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.ime());
            root.setPadding(dp(16) + bars.left, bars.top, dp(16) + bars.right, bars.bottom);
            return insets;
        });
        LinearLayout header = row();
        header.setGravity(Gravity.CENTER_VERTICAL);
        TextView logo = text("⌘", 26, accent);
        logo.setPadding(0, 0, dp(10), 0);
        header.addView(logo);
        LinearLayout brand = col();
        brand.addView(text("ARM STUDIO", 18, ink));
        brand.addView(text("A LITTLE MACHINE. ALL YOURS.", 9, muted));
        header.addView(brand, new LinearLayout.LayoutParams(0, dp(64), 1));
        brand.setGravity(Gravity.CENTER_VERTICAL);
        Button more = button("•••", false);
        more.setContentDescription("Projects and settings");
        more.setOnClickListener(v -> menu(more));
        header.addView(more, new LinearLayout.LayoutParams(dp(48), dp(44)));
        root.addView(header);
        LinearLayout statusRow = row();
        statusRow.setGravity(Gravity.CENTER_VERTICAL);
        TextView dot = text("●", 11, accent);
        statusRow.addView(dot);
        statusView = text("  Ready to make something.", 11, muted);
        statusView.setMaxLines(2);
        statusView.setGravity(Gravity.CENTER_VERTICAL);
        statusRow.addView(statusView, new LinearLayout.LayoutParams(0, dp(36), 1));
        root.addView(statusRow);
        body = col();
        root.addView(body, new LinearLayout.LayoutParams(-1, 0, 1));
        nav = row();
        nav.setPadding(0, dp(8), 0, dp(8));
        String[] labels = {"Code", "Machine", "Inspect", "Guide"};
        for (int j = 0; j < 4; j++) {
            final int n = j;
            Button b = button(labels[j], false);
            b.setTextSize(11);
            b.setSingleLine(true);
            b.setOnClickListener(v -> showTab(n));
            nav.addView(b, new LinearLayout.LayoutParams(0, dp(48), 1));
        }
        root.addView(nav);
        buildEditor();
        buildRun();
        buildInspect();
        buildGuide();
        showTab(0);
        handler.postDelayed(lintTask, 150);
    }
    LinearLayout col() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        return l;
    }
    LinearLayout row() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.HORIZONTAL);
        return l;
    }
    TextView text(String s, int size, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(size);
        t.setTextColor(color);
        t.setIncludeFontPadding(false);
        return t;
    }
    GradientDrawable shape(int color, int stroke, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(radius));
        if (stroke != 0)
            d.setStroke(dp(1), stroke);
        return d;
    }
    Button button(String s, boolean primary) {
        Button b = new Button(this);
        b.setStateListAnimator(null);
        b.setText(s);
        b.setTextSize(12);
        b.setAllCaps(false);
        b.setTextColor(primary ? bg : ink);
        b.setBackground(shape(primary ? accent : raised, 0, 12));
        b.setMinHeight(0);
        b.setMinimumHeight(0);
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setPadding(dp(10), 0, dp(10), 0);
        return b;
    }
    void gap(LinearLayout l, int h) {
        View v = new View(this);
        l.addView(v, new LinearLayout.LayoutParams(1, dp(h)));
    }
    void controls(LinearLayout parent, Button... buttons) {
        LinearLayout r = row();
        for (Button b : buttons) {
            LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(44), 1);
            p.setMargins(0, 0, dp(5), 0);
            r.addView(b, p);
        }
        parent.addView(r);
    }
    TextView section(String title, String subtitle, LinearLayout p) {
        TextView t = text(title, 24, ink);
        t.setTypeface(null, Typeface.BOLD);
        p.addView(t);
        gap(p, 6);
        p.addView(text(subtitle, 12, muted));
        gap(p, 16);
        return t;
    }
    void buildEditor() {
        editorPage = col();
        projectTitle = section(project, "ARM64  /  .s     •     Local draft", editorPage);
        Button assemble = button("Assemble", false);
        assemble.setOnClickListener(v -> assemble());
        runButton = button("▶ Run", true);
        runButton.setOnClickListener(v -> {
            if (running)
                pause();
            else
                start(true);
        });
        Button step = button("Step →", false);
        step.setOnClickListener(v -> step());
        controls(editorPage, assemble, runButton, step);
        gap(editorPage, 12);
        LinearLayout codeCard = col();
        codeCard.setBackground(shape(surface, border, 14));
        codeCard.setClipToOutline(true);
        codeScroll = new ScrollView(this);
        codeScroll.setFillViewport(true);
        LinearLayout codeRow = row();
        gutter = new Gutter();
        codeRow.addView(gutter, new LinearLayout.LayoutParams(dp(43), -1));
        HorizontalScrollView horizontal = new HorizontalScrollView(this);
        horizontal.setFillViewport(true);
        editor = new CodeEdit();
        editor.setTextSize(editorSize);
        editor.setTypeface(Typeface.MONOSPACE);
        editor.setTextColor(ink);
        editor.setBackgroundColor(Color.TRANSPARENT);
        editor.setGravity(Gravity.TOP | Gravity.LEFT);
        editor.setPadding(dp(4), dp(16), dp(24), dp(40));
        editor.setSingleLine(false);
        editor.setHorizontallyScrolling(true);
        editor.setInputType(android.text.InputType.TYPE_CLASS_TEXT |
                            android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE |
                            android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        editor.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        editor.setText(source);
        editor.setMinimumWidth(dp(600));
        editor.setSelectAllOnFocus(false);
        horizontal.addView(editor, new HorizontalScrollView.LayoutParams(-1, -1));
        codeRow.addView(horizontal, new LinearLayout.LayoutParams(0, -1, 1));
        codeScroll.addView(codeRow, new ScrollView.LayoutParams(-1, -1));
        codeCard.addView(codeScroll, new LinearLayout.LayoutParams(-1, 0, 1));
        editorPage.addView(codeCard, new LinearLayout.LayoutParams(-1, 0, 1));
        suggestion = text("Suggestions appear as you type", 12, muted);
        suggestion.setTypeface(Typeface.MONOSPACE);
        suggestion.setSingleLine(true);
        suggestion.setEllipsize(TextUtils.TruncateAt.END);
        suggestion.setGravity(Gravity.CENTER_VERTICAL);
        suggestion.setPadding(dp(12), 0, dp(12), 0);
        suggestion.setBackground(shape(raised, 0, 10));
        LinearLayout.LayoutParams sug = new LinearLayout.LayoutParams(-1, dp(40));
        sug.topMargin = dp(8);
        editorPage.addView(suggestion, sug);
        suggestion.setOnClickListener(v -> acceptCompletion());
        diagnostic = text("Checking assembly…", 11, muted);
        diagnostic.setPadding(dp(2), dp(8), 0, dp(8));
        diagnostic.setMaxLines(3);
        editorPage.addView(diagnostic);
        editor.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
            public void onTextChanged(CharSequence s, int st, int before, int count) {
                if (changing)
                    return;
                pause();
                dirty = true;
                generation++;
                breakpoints.clear();
                Native.breakpoints(new int[0]);
                lintError = "";
                prefs.edit().putString("draft", s.toString()).apply();
                handler.removeCallbacks(lintTask);
                handler.postDelayed(lintTask, 500);
            }
            public void afterTextChanged(Editable e) {
                if (!changing) {
                    highlight();
                    suggest();
                    gutter.invalidate();
                }
            }
        });
        highlight();
    }
    void highlight() {
        if (editor == null || changing)
            return;
        changing = true;
        Editable e = editor.getText();
        for (ForegroundColorSpan s : e.getSpans(0, e.length(), ForegroundColorSpan.class))
            e.removeSpan(s);
        for (UnderlineSpan s : e.getSpans(0, e.length(), UnderlineSpan.class))
            e.removeSpan(s);
        String all = e.toString();
        Matcher lines = Pattern.compile("(?m)^.*$").matcher(all);
        while (lines.find()) {
            String line = lines.group();
            int off = lines.start(), cut = line.length();
            boolean quote = false, escape = false;
            for (int j = 0; j < line.length(); j++) {
                char c = line.charAt(j);
                if (c == '\"' && !escape)
                    quote = !quote;
                if (!quote && (c == ';' ||
                               (c == '/' && j + 1 < line.length() && line.charAt(j + 1) == '/'))) {
                    cut = j;
                    break;
                }
                escape = c == '\\' && !escape;
            }
            String code = line.substring(0, cut);
            Matcher label = Pattern.compile("^\\s*[A-Za-z_][\\w]*:").matcher(code);
            int instructionOffset = label.find() ? label.end() : 0;
            Matcher words = Pattern.compile("^\\s*(?:[A-Za-z_][\\w]*:\\s*)?([.A-Za-z][.\\w]*)")
                                .matcher(code.substring(instructionOffset));
            if (words.find()) {
                String word = words.group(1);
                boolean known = word.startsWith(".") ||
                                catalog.stream().anyMatch(r -> r[0].equalsIgnoreCase(word));
                e.setSpan(new ForegroundColorSpan(known ? accent : error),
                          off + instructionOffset + words.start(1),
                          off + instructionOffset + words.end(1), 33);
                if (!known)
                    e.setSpan(new UnderlineSpan(), off + instructionOffset + words.start(1),
                              off + instructionOffset + words.end(1), 33);
            }
            Matcher tokens = Pattern
                                 .compile("\\b(?:[xw][0-9]+|[ds][0-9]+|sp|lr|fp|[xw]zr)\\b|#(?:-?" +
                                          "0x[0-9a-fA-F]+|-?[0-9]+)|\"(?:\\\\.|[^\"\\\\])*\"")
                                 .matcher(code);
            while (tokens.find())
                e.setSpan(new ForegroundColorSpan(
                              tokens.group().startsWith("\"")  ? Color.rgb(183, 153, 92)
                              : tokens.group().startsWith("#") ? Color.rgb(177, 134, 214)
                                                               : muted),
                          off + tokens.start(), off + tokens.end(), 33);
            if (cut < line.length())
                e.setSpan(new ForegroundColorSpan(muted), off + cut, off + line.length(), 33);
        }
        Matcher err = Pattern.compile("Line (\\d+):").matcher(lintError);
        if (err.find()) {
            int target = Integer.parseInt(err.group(1));
            int start = 0;
            for (int n = 1; n < target && start < all.length(); n++) {
                int end = all.indexOf('\n', start);
                start = end < 0 ? all.length() : end + 1;
            }
            int end = all.indexOf('\n', start);
            if (end < 0)
                end = all.length();
            if (end > start)
                e.setSpan(new UnderlineSpan(), start, end, 33);
        }
        changing = false;
    }
    void suggest() {
        if (editor == null || suggestion == null)
            return;
        completion = "";
        int pos = editor.getSelectionStart();
        String all = editor.getText().toString();
        if (pos < 0 || pos != editor.getSelectionEnd())
            return;
        int start = all.lastIndexOf('\n', Math.max(0, pos - 1)) + 1;
        String before = all.substring(start, pos);
        Matcher m = Pattern.compile("\\s*([a-zA-Z][a-zA-Z.]*)").matcher(before);
        if (m.matches() && (pos == all.length() || all.charAt(pos) == '\n')) {
            String prefix = m.group(1).toLowerCase(Locale.ROOT);
            for (String[] r : catalog)
                if (r[0].startsWith(prefix)) {
                    completion = r[3];
                    prefixStart = start + m.start(1);
                    prefixEnd = pos;
                    break;
                }
        }
        suggestion.setText(completion.isEmpty() ? "Tap a line number to set a breakpoint"
                                                : "↳  " + completion + "    · tap to insert");
        editor.invalidate();
    }
    void acceptCompletion() {
        if (!completion.isEmpty()) {
            String c = completion;
            int at = prefixStart;
            editor.getText().replace(prefixStart, prefixEnd, c);
            editor.setSelection(at + c.length());
        }
    }
    class CodeEdit extends EditText {
        Paint ghost = new Paint(Paint.ANTI_ALIAS_FLAG);
        CodeEdit() {
            super(MainActivity.this);
        }
        @Override
        protected void onSelectionChanged(int a, int b) {
            super.onSelectionChanged(a, b);
            if (editor != null)
                suggest();
        }
        @Override
        protected void onDraw(Canvas c) {
            super.onDraw(c);
            if (!completion.isEmpty() && getLayout() != null && getSelectionStart() == prefixEnd) {
                int pos = getSelectionStart();
                int ln = getLayout().getLineForOffset(pos);
                String typed = getText().toString().substring(prefixStart, prefixEnd);
                if (completion.startsWith(typed)) {
                    ghost.setTypeface(Typeface.MONOSPACE);
                    ghost.setTextSize(getTextSize());
                    ghost.setColor(muted);
                    ghost.setAlpha(130);
                    float x = getTotalPaddingLeft() + getLayout().getPrimaryHorizontal(pos);
                    float y = getTotalPaddingTop() + getLayout().getLineBaseline(ln);
                    c.save();
                    c.clipRect(getScrollX(), 0, getScrollX() + getWidth() - getPaddingRight(),
                               getHeight());
                    c.drawText(completion.substring(typed.length()), x, y, ghost);
                    c.restore();
                }
            }
        }
    }
    class Gutter extends View {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        Gutter() {
            super(MainActivity.this);
            setContentDescription("Line numbers. Tap to toggle breakpoints.");
        }
        @Override
        protected void onDraw(Canvas c) {
            super.onDraw(c);
            if (editor == null || editor.getLayout() == null)
                return;
            p.setTypeface(Typeface.MONOSPACE);
            p.setTextSize(editor.getTextSize() * .8f);
            p.setTextAlign(Paint.Align.RIGHT);
            Layout l = editor.getLayout();
            for (int j = 0; j < l.getLineCount(); j++) {
                float y = editor.getTotalPaddingTop() + l.getLineBaseline(j);
                if (!dirty && Native.line() == j + 1) {
                    p.setColor(raised);
                    c.drawRect(0, y - editor.getLineHeight() + dp(4), getWidth(), y + dp(4), p);
                }
                if (breakpoints.contains(j + 1)) {
                    p.setColor(error);
                    c.drawCircle(dp(7), y - dp(4), dp(3), p);
                }
                p.setColor(!dirty && Native.line() == j + 1 ? accent : muted);
                c.drawText("" + (j + 1), getWidth() - dp(7), y, p);
            }
        }
        @Override
        public boolean onTouchEvent(android.view.MotionEvent e) {
            if (e.getAction() == MotionEvent.ACTION_UP && editor.getLayout() != null) {
                int line = editor.getLayout().getLineForVertical(
                               Math.max(0, (int)e.getY() - editor.getTotalPaddingTop())) +
                           1;
                if (!breakpoints.add(line))
                    breakpoints.remove(line);
                syncBreakpoints();
                invalidate();
                performClick();
                return true;
            }
            return true;
        }
        @Override
        public boolean performClick() {
            super.performClick();
            return true;
        }
    }
    void syncBreakpoints() {
        Native.breakpoints(breakpoints.stream().mapToInt(Integer::intValue).toArray());
    }
    void buildRun() {
        runPage = col();
        section("The machine", "320 × 240    /    Native ARM64", runPage);
        Button reset = button("Reset", false);
        reset.setOnClickListener(v -> assemble());
        playButton = button("▶ Run", true);
        playButton.setOnClickListener(v -> {
            if (running)
                pause();
            else
                start(false);
        });
        Button step = button("Step →", false);
        step.setOnClickListener(v -> step());
        controls(runPage, reset, playButton, step);
        gap(runPage, 12);
        ScrollView scroll = new ScrollView(this);
        LinearLayout inner = col();
        scroll.addView(inner);
        runPage.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        modeLabel = text("TEXT  /  40 × 30 CELLS", 10, accent);
        modeLabel.setPadding(dp(2), 0, 0, dp(8));
        inner.addView(modeLabel);
        frame = new FrameView();
        frame.setBackground(shape(Color.BLACK, border, 10));
        frame.setClipToOutline(true);
        inner.addView(frame, new LinearLayout.LayoutParams(-1, dp(260)));
        gap(inner, 8);
        inner.addView(text("Touch the display to send a single pointer.", 11, muted));
        gap(inner, 16);
        TextView keys = text("VIRTUAL KEYBOARD", 10, muted);
        inner.addView(keys);
        gap(inner, 8);
        for (String letters : new String[] {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"}) {
            LinearLayout r = row();
            for (char c : letters.toCharArray()) {
                Button b = keyButton(String.valueOf(c), c);
                LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(38), 1);
                p.setMargins(0, 0, dp(3), dp(4));
                r.addView(b, p);
            }
            inner.addView(r);
        }
        LinearLayout r = row();
        String[] labels = {"Esc", "←", "↑", "↓", "→", "Space", "↵", "⌫"};
        int[] codes = {27, 258, 256, 257, 259, 32, 10, 8};
        for (int j = 0; j < labels.length; j++) {
            Button b = keyButton(labels[j], codes[j]);
            LinearLayout.LayoutParams p =
                new LinearLayout.LayoutParams(0, dp(40), j == 5 ? 1.5f : 1);
            p.setMargins(0, 0, dp(3), 0);
            r.addView(b, p);
        }
        inner.addView(r);
        gap(inner, 18);
        inner.addView(text("CONSOLE", 10, muted));
        gap(inner, 8);
        console = text("Output appears here.", 12, muted);
        console.setTypeface(Typeface.MONOSPACE);
        console.setTextIsSelectable(true);
        console.setPadding(dp(12), dp(12), dp(12), dp(12));
        console.setBackground(shape(surface, border, 12));
        inner.addView(console);
        gap(inner, 12);
    }
    Button keyButton(String label, int code) {
        Button b = button(label, false);
        b.setTextSize(12);
        b.setPadding(0, 0, 0, 0);
        b.setOnTouchListener((v, e) -> {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                key = code;
                keyDown = true;
                sendInput();
                v.setAlpha(.55f);
                v.getParent().requestDisallowInterceptTouchEvent(true);
                return true;
            }
            if (e.getActionMasked() == MotionEvent.ACTION_UP ||
                e.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                keyDown = false;
                sendInput();
                v.setAlpha(1);
                if (e.getActionMasked() == MotionEvent.ACTION_UP)
                    v.performClick();
                return true;
            }
            return true;
        });
        return b;
    }
    void sendInput() {
        Native.input(key, keyDown, touchX, touchY, touchDown);
    }
    class FrameView extends View {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        Bitmap bitmap = Bitmap.createBitmap(320, 240, Bitmap.Config.ARGB_8888);
        int[] data = new int[1200];
        int mode = 0, primary = -1;
        final int[] palette = {0xff101518, 0xffcc545d, 0xff5dbb83, 0xffd5bb65,
                               0xff658bd6, 0xffb37dd2, 0xff61bfc7, 0xffd1ddd6,
                               0xff52635f, 0xffff8288, 0xff87e9ab, 0xfff9df8d,
                               0xff91b5ff, 0xffdeb2f5, 0xff9fe8ea, 0xfff5fff7};
        FrameView() {
            super(MainActivity.this);
            setContentDescription("VM framebuffer, 320 by 240. Touch to send coordinates.");
        }
        void update() {
            mode = Native.mode();
            data = Native.frame();
            if (mode != 0)
                bitmap.setPixels(data, 0, 320, 0, 0, 320, 240);
            invalidate();
        }
        @Override
        protected void onMeasure(int w, int h) {
            int width = MeasureSpec.getSize(w);
            setMeasuredDimension(width, width * 3 / 4);
        }
        @Override
        protected void onDraw(Canvas c) {
            c.save();
            c.scale(getWidth() / 320f, getHeight() / 240f);
            c.drawColor(palette[0]);
            if (mode == 0) {
                paint.setTypeface(Typeface.MONOSPACE);
                paint.setTextSize(8);
                paint.setTextAlign(Paint.Align.LEFT);
                for (int j = 0; j < data.length; j++) {
                    int cell = data[j], ch = cell & 255;
                    float x = (j % 40) * 8, y = (j / 40) * 8;
                    paint.setColor(palette[(cell >> 16) & 15]);
                    c.drawRect(x, y, x + 8, y + 8, paint);
                    if (ch >= 32 && ch < 127) {
                        paint.setColor(palette[(cell >> 8) & 15]);
                        c.drawText(String.valueOf((char)ch), x + 1, y + 7, paint);
                    }
                }
            } else {
                paint.setFilterBitmap(false);
                c.drawBitmap(bitmap, 0, 0, paint);
            }
            c.restore();
        }
        @Override
        public boolean onTouchEvent(MotionEvent e) {
            int action = e.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                primary = e.getPointerId(0);
                touchDown = true;
                getParent().requestDisallowInterceptTouchEvent(true);
            }
            int idx = e.findPointerIndex(primary);
            if (idx >= 0) {
                touchX = Math.max(0, Math.min(319, (int)(e.getX(idx) * 320 / getWidth())));
                touchY = Math.max(0, Math.min(239, (int)(e.getY(idx) * 240 / getHeight())));
            }
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL ||
                (action == MotionEvent.ACTION_POINTER_UP &&
                 e.getPointerId(e.getActionIndex()) == primary)) {
                touchDown = false;
                primary = -1;
                performClick();
            }
            sendInput();
            return true;
        }
        @Override
        public boolean performClick() {
            super.performClick();
            return true;
        }
    }
    void buildInspect() {
        inspectPage = col();
        section("Under the hood", "Live registers and deterministic virtual memory", inspectPage);
        LinearLayout toolbar = row();
        Button step = button("Step →", false);
        step.setOnClickListener(v -> step());
        Button refresh = button("Refresh", false);
        refresh.setOnClickListener(v -> refresh());
        controls(inspectPage, step, refresh);
        gap(inspectPage, 12);
        ScrollView scroll = new ScrollView(this);
        LinearLayout inner = col();
        scroll.addView(inner);
        inspectPage.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        inner.addView(text("MEMORY", 10, accent));
        gap(inner, 8);
        LinearLayout addr = row();
        address = new EditText(this);
        address.setSingleLine(true);
        address.setText("0x00010000");
        address.setTextSize(14);
        address.setTextColor(ink);
        address.setHintTextColor(muted);
        address.setTypeface(Typeface.MONOSPACE);
        address.setContentDescription("Memory start address in hexadecimal");
        addr.addView(address, new LinearLayout.LayoutParams(0, dp(48), 1));
        Button go = button("Go", false);
        go.setOnClickListener(v -> {
            try {
                String s = address.getText().toString().trim().replaceFirst("(?i)^0x", "");
                long n = Long.parseLong(s, 16);
                if (n < 0 || n > 0xffffffffL)
                    throw new NumberFormatException();
                memoryAddress = (int)n;
                refresh();
            } catch (Exception ex) {
                toast("Enter a 32-bit hexadecimal address");
            }
        });
        addr.addView(go, new LinearLayout.LayoutParams(dp(64), dp(44)));
        inner.addView(addr);
        LinearLayout jumps = row();
        String[] names = {"Code", "RAM", "Screen", "Input"};
        int[] addrs = {0, 0x10000, 0x100000, 0x150000};
        for (int j = 0; j < 4; j++) {
            final int a = addrs[j];
            Button b = button(names[j], false);
            b.setOnClickListener(v -> {
                memoryAddress = a;
                address.setText(String.format(Locale.ROOT, "0x%08X", a));
                refresh();
            });
            LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(36), 1);
            p.rightMargin = dp(4);
            jumps.addView(b, p);
        }
        inner.addView(jumps);
        gap(inner, 8);
        HorizontalScrollView hs = new HorizontalScrollView(this);
        memoryView = text("", 11, ink);
        memoryView.setTypeface(Typeface.MONOSPACE);
        memoryView.setTextIsSelectable(true);
        memoryView.setPadding(dp(12), dp(12), dp(12), dp(12));
        hs.setBackground(shape(surface, border, 10));
        hs.addView(memoryView);
        inner.addView(hs);
        gap(inner, 20);
        inner.addView(text("REGISTERS", 10, accent));
        gap(inner, 8);
        registerView = text("", 13, ink);
        registerView.setTypeface(Typeface.MONOSPACE);
        registerView.setTextIsSelectable(true);
        registerView.setPadding(dp(12), dp(12), dp(12), dp(12));
        registerView.setBackground(shape(surface, border, 10));
        inner.addView(registerView);
        gap(inner, 12);
    }
    void buildGuide() {
        guidePage = col();
        section("Field guide", "Instruction reference, VM calls, and the memory map", guidePage);
        LinearLayout options = row();
        Button reference = button("Instructions", true), manual = button("VM manual", false);
        controls(guidePage, reference, manual);
        gap(guidePage, 10);
        LinearLayout guideBody = col();
        guidePage.addView(guideBody, new LinearLayout.LayoutParams(-1, 0, 1));
        reference.setOnClickListener(v -> {
            reference.setBackground(shape(accent, 0, 12));
            reference.setTextColor(bg);
            manual.setBackground(shape(raised, 0, 12));
            manual.setTextColor(ink);
            instructionGuide(guideBody);
        });
        manual.setOnClickListener(v -> {
            manual.setBackground(shape(accent, 0, 12));
            manual.setTextColor(bg);
            reference.setBackground(shape(raised, 0, 12));
            reference.setTextColor(ink);
            guideBody.removeAllViews();
            ScrollView scroll = new ScrollView(this);
            TextView docs = text(Native.docs(), docsSize, ink);
            docs.setLineSpacing(dp(4), 1);
            docs.setTextIsSelectable(true);
            docs.setPadding(dp(8), dp(8), dp(8), dp(24));
            scroll.addView(docs);
            guideBody.addView(scroll);
        });
        instructionGuide(guideBody);
    }
    void instructionGuide(LinearLayout parent) {
        parent.removeAllViews();
        EditText search = new EditText(this);
        search.setSingleLine(true);
        search.setTextSize(docsSize);
        search.setTextColor(ink);
        search.setHintTextColor(muted);
        search.setHint("Find an instruction…");
        parent.addView(search, new LinearLayout.LayoutParams(-1, dp(48)));
        LinearLayout filters = row();
        Spinner category = new Spinner(this), type = new Spinner(this);
        String[] categories = {"All purposes", "Data",      "Memory",         "Math",  "Bits",
                               "Comparison",   "Branching", "Floating point", "System"};
        String[] types = {"All types", "int64", "int32", "float64", "float32", "control"};
        setupSpinner(category, categories);
        setupSpinner(type, types);
        filters.addView(category, new LinearLayout.LayoutParams(0, dp(44), 1));
        filters.addView(type, new LinearLayout.LayoutParams(0, dp(44), 1));
        parent.addView(filters);
        ScrollView scroll = new ScrollView(this);
        LinearLayout list = col();
        scroll.addView(list);
        parent.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        Runnable render = () -> {
            list.removeAllViews();
            String q = search.getText().toString().toLowerCase(Locale.ROOT);
            int count = 0;
            for (String[] r : catalog) {
                if (category.getSelectedItemPosition() > 0 &&
                    !r[1].equals(category.getSelectedItem()))
                    continue;
                if (type.getSelectedItemPosition() > 0 &&
                    !r[2].contains(type.getSelectedItem().toString()))
                    continue;
                if (!(r[0] + " " + r[4]).toLowerCase(Locale.ROOT).contains(q))
                    continue;
                count++;
                LinearLayout card = col();
                card.setPadding(dp(14), dp(14), dp(14), dp(14));
                card.setBackground(shape(surface, border, 12));
                TextView name = text(r[0] + "  ·  " + r[1], docsSize + 1, accent);
                name.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
                card.addView(name);
                gap(card, 6);
                card.addView(text(r[2], Math.max(10, docsSize - 3), muted));
                gap(card, 10);
                TextView syntax = text(r[3], docsSize, ink);
                syntax.setTypeface(Typeface.MONOSPACE);
                card.addView(syntax);
                gap(card, 8);
                TextView desc = text(r[4], docsSize, muted);
                desc.setLineSpacing(dp(2), 1);
                card.addView(desc);
                LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
                p.topMargin = dp(8);
                list.addView(card, p);
            }
            if (count == 0)
                list.addView(text("No matching instructions.", docsSize, muted));
        };
        search.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
            public void onTextChanged(CharSequence s, int st, int b, int c) {
                render.run();
            }
            public void afterTextChanged(Editable e) {}
        });
        AdapterView.OnItemSelectedListener listener = new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> p, View v, int n, long id) {
                render.run();
            }
            public void onNothingSelected(AdapterView<?> p) {}
        };
        category.setOnItemSelectedListener(listener);
        type.setOnItemSelectedListener(listener);
        render.run();
    }
    void setupSpinner(Spinner s, String[] items) {
        ArrayAdapter<String> a =
            new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, items) {
                @Override
                public View getView(int p, View v, android.view.ViewGroup parent) {
                    TextView t = (TextView)super.getView(p, v, parent);
                    t.setTextColor(ink);
                    t.setTextSize(12);
                    return t;
                }
            };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
    }
    void showTab(int n) {
        tab = n;
        body.removeAllViews();
        body.addView(new View[] {editorPage, runPage, inspectPage, guidePage}[n],
                     new LinearLayout.LayoutParams(-1, -1));
        for (int j = 0; j < 4; j++) {
            Button b = (Button)nav.getChildAt(j);
            b.setTextColor(j == n ? accent : muted);
            b.setBackground(shape(j == n ? raised : bg, 0, 12));
        }
        if (n != 0) {
            editor.clearFocus();
            ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE))
                .hideSoftInputFromWindow(editor.getWindowToken(), 0);
        }
        refresh();
    }
    boolean assemble() {
        pause();
        lintError = Native.assemble(editor.getText().toString());
        dirty = !lintError.isEmpty();
        syncBreakpoints();
        diagnostic.setText(dirty ? lintError
                                 : "✓  Assembled successfully   ·   Ready for Run or Step");
        diagnostic.setTextColor(dirty ? error : accent);
        highlight();
        sendInput();
        refresh();
        return !dirty;
    }
    void start(boolean machine) {
        if ((dirty || Native.state() == 2) && !assemble())
            return;
        running = true;
        if (machine)
            showTab(1);
        handler.removeCallbacks(tick);
        handler.post(tick);
        refresh();
    }
    void pause() {
        handler.removeCallbacks(tick);
        if (running) {
            running = false;
            Native.pause();
        }
        if (runButton != null)
            runButton.setText("▶ Run");
        if (playButton != null)
            playButton.setText("▶ Run");
    }
    void step() {
        pause();
        if ((dirty || Native.state() == 2) && !assemble())
            return;
        Native.run(1, true);
        refresh();
    }
    void refresh() {
        if (statusView == null)
            return;
        String s = dirty ? "Draft · assemble to run" : Native.status();
        statusView.setText("  " + s);
        statusView.setTextColor(s.startsWith("Fault") ? error : muted);
        if (runButton != null)
            runButton.setText(running ? "Ⅱ Pause" : "▶ Run");
        if (playButton != null)
            playButton.setText(running ? "Ⅱ Pause" : "▶ Run");
        if (gutter != null)
            gutter.invalidate();
        if (tab == 1 && frame != null) {
            frame.update();
            modeLabel.setText(new String[] {"TEXT  /  40 × 30 CELLS", "RGB565  /  320 × 240",
                                            "RGB888  /  320 × 240"}[Native.mode()]);
            String out = Native.output().replaceAll("\u001B\\[[0-9;]*[A-Za-z]", "");
            console.setText(out.isEmpty() ? "Output appears here." : out);
        }
        if (tab == 2 && memoryView != null) {
            memoryView.setText(Native.memory(memoryAddress));
            registerView.setText(Native.registers());
        }
    }
    void menu(View anchor) {
        PopupMenu p = new PopupMenu(this, anchor);
        String[] actions = {"Examples",  "Save project", "Open project",
                            "Export .s", "Import .s",    "Settings"};
        for (String a : actions)
            p.getMenu().add(a);
        p.setOnMenuItemClickListener(item -> {
            switch (item.getTitle().toString()) {
            case "Examples":
                new AlertDialog.Builder(this)
                    .setTitle("Example projects")
                    .setItems(examples, (d, n) -> loadProject(examples[n], Native.example(n)))
                    .show();
                break;
            case "Save project":
                saveProject();
                break;
            case "Open project":
                openProject();
                break;
            case "Export .s":
                Intent export = new Intent(Intent.ACTION_CREATE_DOCUMENT)
                                    .setType("text/plain")
                                    .addCategory(Intent.CATEGORY_OPENABLE)
                                    .putExtra(Intent.EXTRA_TITLE, project + ".s");
                startActivityForResult(export, 10);
                break;
            case "Import .s":
                Intent imp = new Intent(Intent.ACTION_OPEN_DOCUMENT)
                                 .setType("*/*")
                                 .addCategory(Intent.CATEGORY_OPENABLE);
                startActivityForResult(imp, 11);
                break;
            case "Settings":
                settings();
                break;
            }
            return true;
        });
        p.show();
    }
    void saveProject() {
        EditText name = new EditText(this);
        name.setSingleLine(true);
        name.setText(project);
        new AlertDialog.Builder(this)
            .setTitle("Save project")
            .setView(name)
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Save",
                               (d, w) -> {
                                   String n = name.getText().toString().trim();
                                   if (n.isEmpty()) {
                                       toast("Enter a project name");
                                       return;
                                   }
                                   project = n;
                                   prefs.edit()
                                       .putString("saved:" + n, editor.getText().toString())
                                       .putString("project", n)
                                       .apply();
                                   projectTitle.setText(n);
                                   toast("Saved on this device");
                               })
            .show();
    }
    void openProject() {
        ArrayList<String> names = new ArrayList<>();
        for (String k : prefs.getAll().keySet())
            if (k.startsWith("saved:"))
                names.add(k.substring(6));
        Collections.sort(names);
        if (names.isEmpty()) {
            toast("No saved projects yet. Use Save project.");
            return;
        }
        new AlertDialog.Builder(this)
            .setTitle("Your projects")
            .setItems(
                names.toArray(new String[0]),
                (d, n) -> loadProject(names.get(n), prefs.getString("saved:" + names.get(n), "")))
            .show();
    }
    void loadProject(String name, String code) {
        pause();
        if (!editor.getText().toString().equals(code))
            prefs.edit()
                .putString("saved:Autosave — " + project, editor.getText().toString())
                .apply();
        project = name;
        editor.setText(code);
        editor.setSelection(0);
        projectTitle.setText(name);
        prefs.edit().putString("project", name).putString("draft", code).apply();
        dirty = true;
        breakpoints.clear();
        showTab(0);
    }
    void settings() {
        LinearLayout content = col();
        content.setPadding(dp(24), dp(12), dp(24), dp(12));
        TextView e = text("Editor · " + editorSize + " sp", 15, ink),
                 d = text("Documentation · " + docsSize + " sp", 15, ink);
        content.addView(e);
        SeekBar es = new SeekBar(this);
        es.setMax(16);
        es.setProgress(editorSize - 10);
        content.addView(es);
        gap(content, 18);
        content.addView(d);
        SeekBar ds = new SeekBar(this);
        ds.setMax(14);
        ds.setProgress(docsSize - 12);
        content.addView(ds);
        gap(content, 18);
        content.addView(text("Appearance follows your system theme.", 13, muted));
        es.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int n, boolean user) {
                editorSize = n + 10;
                e.setText("Editor · " + editorSize + " sp");
                editor.setTextSize(editorSize);
                gutter.invalidate();
            }
            public void onStartTrackingTouch(SeekBar s) {}
            public void onStopTrackingTouch(SeekBar s) {}
        });
        ds.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int n, boolean user) {
                docsSize = n + 12;
                d.setText("Documentation · " + docsSize + " sp");
            }
            public void onStartTrackingTouch(SeekBar s) {}
            public void onStopTrackingTouch(SeekBar s) {}
        });
        new AlertDialog.Builder(this)
            .setTitle("Make yourself at home")
            .setView(content)
            .setPositiveButton("Done",
                               (dialog, w) -> {
                                   prefs.edit()
                                       .putInt("editorSize", editorSize)
                                       .putInt("docsSize", docsSize)
                                       .apply();
                                   buildGuide();
                                   if (tab == 3)
                                       showTab(3);
                               })
            .show();
    }
    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null)
            return;
        try {
            if (request == 10) {
                try (OutputStream out = getContentResolver().openOutputStream(data.getData())) {
                    out.write(editor.getText().toString().getBytes(StandardCharsets.UTF_8));
                }
                toast("Exported assembly source");
            } else if (request == 11) {
                try (InputStream in = getContentResolver().openInputStream(data.getData());
                     ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                    byte[] buffer = new byte[4096];
                    int n;
                    while ((n = in.read(buffer)) != -1) {
                        if (out.size() + n > 256 * 1024)
                            throw new IOException("Source exceeds 256 KiB");
                        out.write(buffer, 0, n);
                    }
                    loadProject("Imported program", out.toString("UTF-8"));
                }
            }
        } catch (Exception ex) {
            toast("File operation failed: " + ex.getMessage());
        }
    }
    void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_LONG).show();
    }
    @Override
    protected void onPause() {
        super.onPause();
        pause();
        keyDown = touchDown = false;
        sendInput();
        if (editor != null)
            prefs.edit()
                .putString("draft", editor.getText().toString())
                .putString("project", project)
                .putInt("editorSize", editorSize)
                .putInt("docsSize", docsSize)
                .apply();
    }
    @Override
    protected void onDestroy() {
        destroyed = true;
        handler.removeCallbacksAndMessages(null);
        lintWorker.shutdownNow();
        super.onDestroy();
    }
}
