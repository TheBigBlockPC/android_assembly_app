package dev.armstudio;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/** Device UI integration checks. Restores the user's projects and draft in finally. */
public class SmokeInstrumentation extends Instrumentation {
    MainActivity activity;
    int checks;
    interface Work {
        void run() throws Exception;
    }
    void main(Work work) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        runOnMainSync(() -> {
            try {
                work.run();
            } catch (Throwable e) {
                failure.set(e);
            }
        });
        if (failure.get() != null)
            throw new Exception(failure.get());
    }
    void check(boolean pass, String message) {
        checks++;
        if (!pass)
            throw new AssertionError(message);
    }
    void source(String code) throws Exception {
        main(() -> {
            activity.pause();
            activity.showTab(0);
            activity.editor.setText(code);
            activity.editor.setSelection(code.length());
        });
    }
    void waitForIdle() throws Exception {
        Thread.sleep(150);
        waitForIdleSync();
    }
    Button findButton(View view, String label) {
        if (view instanceof Button && ((Button)view).getText().toString().equals(label))
            return (Button)view;
        if (view instanceof ViewGroup)
            for (int n = 0; n < ((ViewGroup)view).getChildCount(); n++) {
                Button result = findButton(((ViewGroup)view).getChildAt(n), label);
                if (result != null)
                    return result;
            }
        return null;
    }
    @Override
    public void onCreate(Bundle args) {
        super.onCreate(args);
        start();
    }
    @Override
    public void onStart() {
        SharedPreferences prefs = getTargetContext().getSharedPreferences("studio", 0);
        Map<String, ?> saved = prefs.getAll();
        Bundle result = new Bundle();
        try {
            Intent intent = new Intent(getTargetContext(), MainActivity.class)
                                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity = (MainActivity)startActivitySync(intent);
            waitForIdle();
            source(Native.example(0));
            main(() -> {
                activity.runButton.performClick();
                check(activity.tab == 1, "Run opens Machine");
            });
            waitForIdle();
            main(() -> {
                check(Native.status().startsWith("Finished"), "Hello world finishes");
                check(Native.output().contains("Hello, ARM64!"), "Hello world output");
                check(activity.frame.data.length == 1200, "Text framebuffer is 40x30");
                check(activity.frame.getWidth() * 3 / 4 == activity.frame.getHeight(),
                      "Framebuffer keeps 4:3 aspect");
                activity.showTab(2);
                check(activity.memoryView.getText().toString().contains("00010000"),
                      "Memory inspector");
                check(activity.registerView.getText().toString().contains("NZCV"),
                      "Register inspector");
            });
            source(".text\nld");
            main(() -> {
                activity.suggest();
                check(activity.completion.startsWith("ldr "), "ld completion");
                activity.suggestion.performClick();
                check(activity.editor.getText().toString().contains("ldr x0, [x1, #8]"),
                      "Completion insertion");
            });
            source(".text\nmessage:\n    nop\n");
            Thread.sleep(800);
            main(() -> {
                check(activity.lintError.isEmpty(), "Valid label lint");
                Spanned text = activity.editor.getText();
                for (ForegroundColorSpan span : text.getSpans(6, 13, ForegroundColorSpan.class))
                    check(span.getForegroundColor() != activity.error,
                          "Label is not marked invalid");
            });
            source(".text\n    ldzz x0, [x1]\n");
            Thread.sleep(800);
            main(() -> {
                check(activity.lintError.contains("Unknown or unsupported"),
                      "Unknown mnemonic lint diagnostic");
                Spanned text = activity.editor.getText();
                boolean red = false;
                for (ForegroundColorSpan span : text.getSpans(10, 14, ForegroundColorSpan.class))
                    red |= span.getForegroundColor() == activity.error;
                check(red, "Invalid mnemonic is red");
            });
            source("mov x0, #1\nadd x0, x0, #1\nhlt #0\n");
            waitForIdle();
            main(() -> {
                float y = activity.editor.getTotalPaddingTop() +
                          activity.editor.getLayout().getLineTop(1) + 2;
                MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 15, y, 0);
                activity.gutter.onTouchEvent(event);
                event.recycle();
                check(activity.breakpoints.contains(2), "Gutter sets breakpoint");
                activity.runButton.performClick();
            });
            waitForIdle();
            main(() -> {
                check(Native.status().startsWith("Breakpoint"), "Run hits editor breakpoint");
                check(Native.line() == 2, "Breakpoint source line");
                activity.step();
                check(Native.line() == 3, "Step advances one line");
            });
            source("mov x0, #0\nstr x1, [x0]\n");
            main(() -> activity.runButton.performClick());
            waitForIdle();
            main(() -> {
                check(Native.status().startsWith("Fault"),
                      "Bad memory access safely faults in app");
                activity.showTab(2);
                check(activity.registerView.getText().length() > 0,
                      "App remains usable after fault");
            });
            source(Native.example(2));
            main(() -> activity.runButton.performClick());
            waitForIdle();
            main(() -> {
                float x = activity.frame.getWidth() * 100f / 320,
                      y = activity.frame.getHeight() * 80f / 240;
                MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x, y, 0);
                activity.frame.dispatchTouchEvent(event);
                event.recycle();
                check(activity.touchDown, "Framebuffer pointer goes down");
            });
            Thread.sleep(120);
            main(() -> {
                int offset = 0x100000 + (activity.touchY * 320 + activity.touchX) * 4;
                check(Native.memory(offset).contains("bf e2 74 00"),
                      "Real View touch paints guest framebuffer");
                MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
                activity.frame.dispatchTouchEvent(event);
                event.recycle();
                check(!activity.touchDown, "Framebuffer pointer goes up");
                activity.pause();
            });
            source(Native.example(3));
            main(() -> activity.runButton.performClick());
            waitForIdle();
            main(() -> {
                Button a = findButton(activity.runPage, "a");
                check(a != null, "Virtual keyboard exists");
                MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 5, 5, 0);
                a.dispatchTouchEvent(event);
                event.recycle();
            });
            Thread.sleep(120);
            main(() -> {
                check(Native.output().endsWith("a"), "Virtual keyboard delivers ASCII to guest");
                Button a = findButton(activity.runPage, "a");
                MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 5, 5, 0);
                a.dispatchTouchEvent(event);
                event.recycle();
                activity.pause();
                activity.showTab(3);
                check(activity.guidePage.getChildCount() > 0, "Instruction guide renders");
                Button manual = findButton(activity.guidePage, "VM manual");
                manual.performClick();
                check(activity.guidePage.getChildCount() > 0, "VM manual renders");
            });
            source("nop\n");
            main(() -> {
                activity.project = "Smoke test draft";
                activity.assemble();
                activity.loadProject("Hello world", Native.example(0));
                check(prefs.getString("saved:Autosave — Smoke test draft", "").equals("nop\n"),
                      "Switching projects preserves assembled unsaved source");
            });
            result.putString("stream", "\nPASS: " + checks + " UI integration checks\n");
        } catch (Throwable e) {
            result.putString("stream", "\nFAIL after " + checks + " UI checks: " +
                                           android.util.Log.getStackTraceString(e));
            result.putString("failure", e.toString());
        } finally {
            try {
                if (activity != null)
                    main(() -> {
                        activity.pause();
                        activity.editor.setText(saved.containsKey("draft")
                                                    ? (String)saved.get("draft")
                                                    : Native.example(0));
                        activity.project = saved.containsKey("project")
                                               ? (String)saved.get("project")
                                               : "Hello world";
                        activity.projectTitle.setText(activity.project);
                        activity.showTab(0);
                        activity.finish();
                    });
                waitForIdleSync();
                SharedPreferences.Editor restore = prefs.edit().clear();
                for (Map.Entry<String, ?> entry : saved.entrySet()) {
                    Object value = entry.getValue();
                    if (value instanceof String)
                        restore.putString(entry.getKey(), (String)value);
                    else if (value instanceof Integer)
                        restore.putInt(entry.getKey(), (Integer)value);
                    else if (value instanceof Boolean)
                        restore.putBoolean(entry.getKey(), (Boolean)value);
                }
                restore.commit();
            } catch (Throwable e) {
                result.putString("failure", "Restore failed: " + e);
            }
        }
        finish(result.containsKey("failure") ? Activity.RESULT_CANCELED : Activity.RESULT_OK,
               result);
    }
}
