package dev.armstudio;
public final class Native {
    static {
        System.loadLibrary("armstudio");
    }
    public static native String assemble(String source);
    public static native String lint(String source);
    public static native void run(int budget, boolean single);
    public static native void pause();
    public static native void breakpoints(int[] lines);
    public static native int state();
    public static native int line();
    public static native int mode();
    public static native boolean yielded();
    public static native long count();
    public static native String[] instructions(boolean lastExecuted);
    public static native String status();
    public static native String registers();
    public static native String memory(int address);
    public static native String output();
    public static native int[] frame();
    public static native void input(int key, boolean down, int x, int y, boolean touch);
    public static native String catalog();
    public static native String docs();
    public static native String[] manual();
    public static native String example(int n);
}
