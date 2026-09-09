package dev.armstudio;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import java.io.FileOutputStream;
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
    String allText(View view) {
        StringBuilder result = new StringBuilder();
        if (view instanceof TextView)
            result.append(((TextView)view).getText()).append("\n");
        if (view instanceof ViewGroup)
            for (int n = 0; n < ((ViewGroup)view).getChildCount(); ++n)
                result.append(allText(((ViewGroup)view).getChildAt(n)));
        return result.toString();
    }
    void capture(String name) throws Exception {
        Bitmap bitmap = getUiAutomation().takeScreenshot();
        if (bitmap != null)
            try (FileOutputStream out = getTargetContext().openFileOutput(name, 0)) {
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
            }
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
            main(() -> activity.setExecutionMode(MainActivity.DEFAULT));
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
                check(activity.tab == 2 && activity.inspectorLine == 2,
                      "Breakpoint automatically opens the current source instruction");
                check(allText(activity.instructionRows).contains("add x0, x0, #1"),
                      "Inspector includes actual instruction text");
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
                check(activity.inspectorLine == 2 &&
                          activity.instructionHeading.getText().toString().startsWith("Faulting"),
                      "Inspector marks the faulting source instruction");
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
            // V2 execution controls and inspector state must agree before/after each pause.
            source("mov x0, #0\nloop:\nadd x0, x0, #1\nb loop\n");
            main(() -> {
                activity.modeButtons.get(MainActivity.SLOW).performClick();
                activity.speedSlider.setProgress(0);
                check(activity.slowControls.getVisibility() == View.VISIBLE &&
                          activity.slowRate == 1,
                      "Slow mode exposes a 1 instruction/second slider");
                activity.runButton.performClick();
                check(activity.tab == 2, "Slow Run opens Inspect");
            });
            Thread.sleep(200);
            main(() -> {
                check(Native.count() == 1,
                      "Slow execution advances one instruction before next interval");
                check(activity.inspectorLine == 1 &&
                          activity.instructionHeading.getText().toString().startsWith(
                              "Just executed"),
                      "Slow inspector follows the last executed instruction");
                activity.inspectRunButton.performClick();
                check(!activity.running && Native.state() == 1 && activity.inspectorLine == 3,
                      "Pause immediately shows next instruction, accounting for labels");
                check(
                    activity.instructionHeading.getText().toString().startsWith("Next instruction"),
                    "Paused context is explicitly before execution");
            });
            capture("v2-inspector.png");
            main(() -> {
                activity.step();
                check(Native.count() == 2 && activity.inspectorLine == 4,
                      "Step refreshes source context");
                activity.speedSlider.setProgress(19);
                check(activity.slowRate == 20 && prefs.getInt("slowRate", 0) == 20,
                      "Slider speed persists");
                activity.start(false);
            });
            Thread.sleep(220);
            final long[] count = new long[1];
            main(() -> {
                check(Native.count() > 2 && Native.count() < 30,
                      "Higher slow rate advances without resetting");
                count[0] = Native.count();
                activity.modeButtons.get(MainActivity.FAST).performClick();
                check(Native.count() == count[0] && activity.running,
                      "Changing speed preserves live state");
                check(activity.slowControls.getVisibility() == View.GONE &&
                          prefs.getInt("executionMode", -1) == MainActivity.FAST,
                      "Fast mode selection persists and hides slow slider");
            });
            Thread.sleep(60);
            main(() -> {
                activity.pauseAndInspect();
                check(Native.count() > count[0] + 100 && !activity.running && Native.state() == 1,
                      "Fast mode advances rapidly and remains pausable");
                check(activity.inspectorLine == Native.line(),
                      "Fast pause shows precise next instruction");
                count[0] = Native.count();
                String registers = Native.registers();
                activity.setExecutionMode(MainActivity.DEFAULT);
                check(Native.registers().equals(registers),
                      "Changing modes while paused preserves all registers");
            });
            Thread.sleep(100);
            main(() -> {
                check(Native.count() == count[0], "No queued execution continues after Pause");
                activity.showTab(3);
                findButton(activity.guidePage, "VM manual").performClick();
                check(activity.manualTopics.getCount() == 12,
                      "Manual provides eleven topics and all-topics view");
                activity.manualTopics.setSelection(5);
            });
            waitForIdle();
            main(() -> {
                String content = allText(activity.manualContent);
                check(content.contains("Memory map") && content.contains("0x00150020"),
                      "Memory topic keeps full map");
                check(activity.manualContent.getChildCount() >= 10,
                      "Memory map is split into readable entry cards");
            });
            capture("v2-manual.png");
            main(() -> {
                activity.manualSearch.setText("RGB565");
                check(allText(activity.manualContent).contains("Framebuffer formats"),
                      "Manual search finds content across topics");
                activity.manualSearch.setText("no_such_manual_entry_123");
                check(allText(activity.manualContent).contains("No matching"),
                      "Manual search has an empty state");
                activity.manualSearch.setText("");
                check(allText(activity.manualContent).contains("Memory map"),
                      "Clearing search restores selected topic");
            });
            source("nop\n");
            main(() -> {
                activity.showTab(2);
                check(activity.instructionRows.getChildCount() == 0 && activity.inspectorLine == 0,
                      "Editing hides stale assembled source context");
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
