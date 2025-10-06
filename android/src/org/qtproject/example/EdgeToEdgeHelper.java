package org.qtproject.example;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.activity.EdgeToEdge;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import java.util.List;

public final class EdgeToEdgeHelper {
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    // WakeLock para Samsung/One UI que ignora KEEP_SCREEN_ON às vezes
    private static PowerManager.WakeLock sWakeLock;

    // === API a chamar do C++ (main.cpp) ===
    public static void installForQt(Activity activity) {
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (activity == null) return;

        // Edge-to-edge
        WindowCompat.setDecorFitsSystemWindows(activity.getWindow(), false);
        EdgeToEdge.enable(activity);

        final ViewGroup content = activity.findViewById(android.R.id.content);
        if (content == null) return;

        // 1) Tenta instalar agora em qualquer filho já existente
        attachInsetsHandlersRecursively(content);

        // 2) Se o Qt adicionar o view depois, capturamos e instalamos na hora:
        content.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override public void onChildViewAdded(View parent, View child) {
                attachInsetsHandlersRecursively(child);
                // pedir o primeiro dispatch assim que o Qt view estiver pronto
                MAIN.post(() -> ViewCompat.requestApplyInsets(child));
            }
            @Override public void onChildViewRemoved(View parent, View child) { /* noop */ }
        });

        // Comportamento recomendado das barras (transientes por swipe)
        WindowInsetsControllerCompat ctl =
                new WindowInsetsControllerCompat(activity.getWindow(), content);
        ctl.setSystemBarsBehavior(WindowInsetsControllerCompat
                .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);

        // Garantir um dispatch inicial (se já houver filho)
        MAIN.post(() -> ViewCompat.requestApplyInsets(content));
    }

    public static void keepScreenOn(Activity activity, boolean on) {
        if (activity == null) return;

        final Window w = activity.getWindow();
        final View decor = w.getDecorView();

        if (on) {
            // 1) Flag padrão
            w.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            // 2) WakeLock (seguro enquanto em foreground)
            acquireWakeLock(activity);
        } else {
            releaseWakeLock();
            w.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    // === Internos ===

    // Aplica listeners de insets/animacao ao alvo "qt view" (ou a todos recursivamente)
    private static void attachInsetsHandlersRecursively(View root) {
        if (root == null) return;

        // Heurística: views do Qt costumam ter classes em "org.qtproject.qt.android"
        // ou nomes contendo "Qt". Mas para garantir, aplicamos ao root e a todos os filhos.
        attachInsetsHandlers(root);

        if (root instanceof ViewGroup) {
            ViewGroup g = (ViewGroup) root;
            for (int i = 0; i < g.getChildCount(); i++) {
                attachInsetsHandlersRecursively(g.getChildAt(i));
            }
        }
    }

    private static void attachInsetsHandlers(View target) {
        // Listener de insets: aplica padding inicial top/bottom/left/right
        ViewCompat.setOnApplyWindowInsetsListener(target, (v, insets) -> {
            final Insets sb = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(sb.left, sb.top, sb.right, sb.bottom);
            return insets; // não consome
        });

        // Animação do bottom enquanto a barra transitória aparece/desaparece
        ViewCompat.setWindowInsetsAnimationCallback(target,
                new WindowInsetsAnimationCompat.Callback(
                        WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_CONTINUE_ON_SUBTREE) {
                    @Override
                    public WindowInsetsCompat onProgress(WindowInsetsCompat insets,
                                                         List<WindowInsetsAnimationCompat> running) {
                        final Insets sb = insets.getInsets(WindowInsetsCompat.Type.systemBars());
                        target.setPadding(target.getPaddingLeft(),
                                          target.getPaddingTop(),
                                          target.getPaddingRight(),
                                          sb.bottom);
                        return insets;
                    }
                });
    }

    private static void acquireWakeLock(Activity a) {
        if (sWakeLock != null && sWakeLock.isHeld()) return;
        PowerManager pm = (PowerManager) a.getSystemService(Context.POWER_SERVICE);
        if (pm == null) return;
        // SCREEN_BRIGHT_WAKE_LOCK é deprecado; PARTIAL mantém CPU acesa.
        // Porém, em conjunto com FLAG_KEEP_SCREEN_ON e estando em foreground,
        // previne a tela de apagar em alguns OEMs.
        sWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Musicool:KeepOn");
        sWakeLock.setReferenceCounted(false);
        try { sWakeLock.acquire(); } catch (Throwable ignore) {}
    }

    private static void releaseWakeLock() {
        try {
            if (sWakeLock != null && sWakeLock.isHeld()) sWakeLock.release();
        } catch (Throwable ignore) {}
        sWakeLock = null;
    }

    public static int[] getSystemBarInsets(Activity activity) {
        final View root = activity.getWindow().getDecorView();
        final WindowInsetsCompat wi = ViewCompat.getRootWindowInsets(root);
        if (wi == null) return new int[]{0,0,0,0};
        final Insets in = wi.getInsets(WindowInsetsCompat.Type.systemBars());
        return new int[]{ in.left, in.top, in.right, in.bottom };
    }

    public static void requestApplyInsets(Activity activity) {
        final View root = activity.getWindow().getDecorView();
        root.requestApplyInsets();
        // redundância útil para Android 15 / One UI:
        View content = root.findViewById(android.R.id.content);
        if (content != null) content.requestApplyInsets();
    }
}
