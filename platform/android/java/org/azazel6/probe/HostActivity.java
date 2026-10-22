package org.azazel6.probe;

/** Synthetic Vulkan/input diagnostic; contains no game program or assets. */
public final class HostActivity extends org.libsdl.app.SDLActivity {
    @Override
    public void onConfigurationChanged(android.content.res.Configuration configuration) {
        super.onConfigurationChanged(configuration);
        // This native diagnostic owns no resource-based UI to reinflate. SDL
        // updates its surface/insets; recreating the Activity would exit SDL.
        android.util.Log.i("Azazel6Host", "Host configuration updated");
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
