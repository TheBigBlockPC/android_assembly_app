package dev.armstudio;

import java.util.*;
import org.json.*;

/** Versioned, self-contained machine configuration; no executable host paths in imports. */
final class MachineConfig {
    static final String[] COMPONENTS = {"Minimal GPU · framebuffer / text",
                                        "Basic GPU · tiles / sprites",
                                        "Advanced GPU · 3D command buffer",
                                        "Timer",
                                        "Touch input controller",
                                        "Keyboard input controller",
                                        "DMA controller",
                                        "RNG",
                                        "Non volatile memory"};
    String name = "Default", id = UUID.randomUUID().toString();
    int width = 320, height = 240;
    int[] types = {1, 4, 5, 6, 7, 8, 9};
    static MachineConfig preset(int preset) {
        MachineConfig m = new MachineConfig();
        if (preset == 1) {
            m.name = "Phone";
            m.height = 640;
            m.types = new int[] {1, 4, 5, 7, 8, 9};
        } else if (preset == 2)
            m.name = "Custom";
        return m;
    }
    boolean has(int type) {
        for (int t : types)
            if (t == type)
                return true;
        return false;
    }
    JSONObject json() throws JSONException {
        return new JSONObject()
            .put("format", "armstudio-machine")
            .put("version", 1)
            .put("name", name)
            .put("id", id)
            .put("width", width)
            .put("height", height)
            .put("components", new JSONArray(types));
    }
    String encode() {
        try {
            return json().toString(2);
        } catch (JSONException e) {
            throw new IllegalStateException(e);
        }
    }
    static MachineConfig parse(JSONObject o) throws JSONException {
        if (!o.getString("format").equals("armstudio-machine") || o.getInt("version") != 1)
            throw new JSONException("Unsupported machine format or version");
        MachineConfig m = new MachineConfig();
        m.name = o.getString("name");
        m.width = o.getInt("width");
        m.height = o.getInt("height");
        m.id = o.getString("id");
        if (!m.id.matches("[a-zA-Z0-9_-]{1,64}") || m.name.length() > 80 || m.name.trim().isEmpty())
            throw new JSONException("Invalid machine name or ID");
        if (m.width < 160 || m.width > 640 || m.height < 120 || m.height > 960 ||
            m.width % 8 != 0 || m.height % 8 != 0)
            throw new JSONException("Display must be multiples of 8: 160–640 × 120–960");
        JSONArray a = o.getJSONArray("components");
        if (a.length() > 9)
            throw new JSONException("Too many components");
        m.types = new int[a.length()];
        HashSet<Integer> seen = new HashSet<>();
        for (int n = 0; n < a.length(); ++n) {
            m.types[n] = a.getInt(n);
            if (m.types[n] < 1 || m.types[n] > 9 || !seen.add(m.types[n]))
                throw new JSONException("Invalid or duplicate component");
        }
        return m;
    }
    MachineConfig copy() {
        try {
            return parse(json());
        } catch (JSONException e) {
            throw new IllegalStateException(e);
        }
    }
}
