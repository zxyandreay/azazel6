package org.azazel6.probe;

/** Synthetic Vulkan/input diagnostic; contains no game program or assets. */
public final class HostActivity extends org.libsdl.app.SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
