#include "libultra_internal.h"
#include "port/interpolation/FrameInterpolation.h"
#include <libultraship.h>
#include <math.h>

// Notch mode: while racing renders into the camera-housing rows, widen the
// vertical field of view by the added-rows ratio so the horizontal view matches
// classic fullscreen exactly and the extra screen height shows MORE world
// instead of proportionally less width.
float NotchExpandFovY(float fovyDegrees) {
#ifdef __APPLE__
    if (CVarGetInteger("gNotchPanelActiveNow", 0) && CVarGetInteger("gNotchFullBleedNow", 0)) {
        float expand = CVarGetFloat("gNotchVertExpand", 1.0f);
        if (expand > 1.0f) {
            float half = fovyDegrees * ((float)M_PI / 360.0f);
            return 2.0f * atanf(tanf(half) * expand) * (180.0f / (float)M_PI);
        }
    }
#endif
    return fovyDegrees;
}

void guPerspectiveF(float mf[4][4], u16* perspNorm, float fovy, float aspect, float near, float far, float scale) {
    fovy = NotchExpandFovY(fovy);
    float yscale;
    int row;
    int col;
    if (CVarGetInteger("gNoCulling", 0)) {
        far = CVarGetFloat("gFarFrustrum", 10000.0f);
    }
    guMtxIdentF(mf);
    fovy *= GU_PI / 180.0;
    yscale = cosf(fovy / 2) / sinf(fovy / 2);
    mf[0][0] = yscale / aspect;
    mf[1][1] = yscale;
    mf[2][2] = (near + far) / (near - far);
    mf[2][3] = -1;
    mf[3][2] = 2 * near * far / (near - far);
    mf[3][3] = 0.0f;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            mf[row][col] *= scale;
        }
    }
    if (perspNorm != NULL) {
        if (near + far <= 2.0) {
            *perspNorm = 65535;
        } else {
            *perspNorm = (double) (1 << 17) / (near + far);
            if (*perspNorm <= 0) {
                *perspNorm = 1;
            }
        }
    }
}

void guPerspective(Mtx* m, u16* perspNorm, float fovy, float aspect, float near, float far, float scale) {
    float mat[4][4];
    guPerspectiveF(mat, perspNorm, fovy, aspect, near, far, scale);
    FrameInterpolation_RecordMatrixMtxFToMtx((MtxF*)mat, m);
    guMtxF2L(mat, m);
}
