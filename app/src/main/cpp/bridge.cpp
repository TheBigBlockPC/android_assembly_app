#include "engine.h"
#include <jni.h>
#include <memory>
using namespace armstudio;
static std::unique_ptr<Engine> vm;
static Engine &engine() {
    if (!vm)
        vm = std::make_unique<Engine>();
    return *vm;
}
static std::string str(JNIEnv *e, jstring s) {
    if (!s)
        return "";
    const char *p = e->GetStringUTFChars(s, nullptr);
    std::string r(p);
    e->ReleaseStringUTFChars(s, p);
    return r;
}
static jstring js(JNIEnv *e, const std::string &s) {
    return e->NewStringUTF(s.c_str());
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_assemble(JNIEnv *e, jclass,
                                                                        jstring s) {
    return js(e, engine().assemble(str(e, s)));
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_lint(JNIEnv *e, jclass, jstring s) {
    Engine checker;
    return js(e, checker.assemble(str(e, s), false));
}
extern "C" JNIEXPORT void JNICALL
Java_dev_armstudio_Native_run(JNIEnv *, jclass, jint n, jboolean single, jboolean timeLimited) {
    engine().run(n, single, timeLimited);
}
extern "C" JNIEXPORT void JNICALL Java_dev_armstudio_Native_pause(JNIEnv *, jclass) {
    engine().stop();
}
extern "C" JNIEXPORT void JNICALL Java_dev_armstudio_Native_breakpoints(JNIEnv *e, jclass,
                                                                        jintArray a) {
    engine().breaks.clear();
    auto p = e->GetIntArrayElements(a, nullptr);
    for (int i = 0; i < e->GetArrayLength(a); i++)
        engine().breaks.insert(p[i]);
    e->ReleaseIntArrayElements(a, p, JNI_ABORT);
}
extern "C" JNIEXPORT jint JNICALL Java_dev_armstudio_Native_state(JNIEnv *, jclass) {
    return engine().halted ? 2 : engine().paused ? 1 : 0;
}
extern "C" JNIEXPORT jint JNICALL Java_dev_armstudio_Native_line(JNIEnv *, jclass) {
    return engine().currentLine();
}
extern "C" JNIEXPORT jint JNICALL Java_dev_armstudio_Native_mode(JNIEnv *, jclass) {
    return engine().mode;
}
extern "C" JNIEXPORT jboolean JNICALL Java_dev_armstudio_Native_yielded(JNIEnv *, jclass) {
    return engine().yielded;
}
extern "C" JNIEXPORT jlong JNICALL Java_dev_armstudio_Native_count(JNIEnv *, jclass) {
    return engine().count;
}
extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_armstudio_Native_instructions(JNIEnv *e, jclass, jboolean lastExecuted) {
    auto rows = engine().instructionWindow(lastExecuted);
    auto result = e->NewObjectArray(rows.size(), e->FindClass("java/lang/String"), nullptr);
    for (size_t n = 0; n < rows.size(); ++n) {
        const auto &r = rows[n];
        auto value =
            js(e, std::to_string(r.current) + "\t" + std::to_string(r.breakpoint) + "\t" +
                      std::to_string(r.line) + "\t" + std::to_string(r.address) + "\t" + r.text);
        e->SetObjectArrayElement(result, n, value);
        e->DeleteLocalRef(value);
    }
    return result;
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_status(JNIEnv *e, jclass) {
    return js(e, engine().status);
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_registers(JNIEnv *e, jclass) {
    return js(e, engine().registers());
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_memory(JNIEnv *e, jclass,
                                                                      jint address) {
    return js(e, engine().memory(address));
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_output(JNIEnv *e, jclass) {
    auto s = engine().output;
    for (char &c : s)
        if ((unsigned char)c >= 128 || c == 0)
            c = '?';
    return js(e, s);
}
extern "C" JNIEXPORT jintArray JNICALL Java_dev_armstudio_Native_frame(JNIEnv *e, jclass) {
    auto p = engine().mode == 0 ? engine().cells() : engine().pixels();
    auto a = e->NewIntArray(p.size());
    e->SetIntArrayRegion(a, 0, p.size(), p.data());
    return a;
}
extern "C" JNIEXPORT void JNICALL Java_dev_armstudio_Native_input(JNIEnv *, jclass, jint k,
                                                                  jboolean down, jint x, jint y,
                                                                  jboolean touch) {
    engine().input(k, down, x, y, touch);
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_catalog(JNIEnv *e, jclass) {
    return js(e, instructionCatalog());
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_docs(JNIEnv *e, jclass) {
    return js(e, documentation());
}
extern "C" JNIEXPORT jobjectArray JNICALL Java_dev_armstudio_Native_manual(JNIEnv *e, jclass) {
    auto sections = manualSections();
    auto result = e->NewObjectArray(sections.size(), e->FindClass("java/lang/String"), nullptr);
    for (size_t n = 0; n < sections.size(); ++n) {
        auto value = js(e, sections[n].title + "\t" + sections[n].body);
        e->SetObjectArrayElement(result, n, value);
        e->DeleteLocalRef(value);
    }
    return result;
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_armstudio_Native_example(JNIEnv *e, jclass, jint n) {
    return js(e, example(n));
}
