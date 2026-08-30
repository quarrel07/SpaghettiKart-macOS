#include "Tree.h"

#include <macros.h>
#include <libultra/gbi.h>
#include <assets/models/tracks/mario_raceway/mario_raceway_data.h>
#include <assets/textures/common_data.h>
#include "port/interpolation/FrameInterpolation.h"
#include "engine/World.h"
#include "port/Game.h"

extern "C" {
#include "common_structs.h"
#include "defines.h"
#include "racing/math_util.h"
#include "main.h"
#include "racing/actors.h"
#include "code_800029B0.h"
}

size_t ATree::_count = 0;

ATree* ATree::Spawn(FVector pos, TreeKind kind) {
    return static_cast<ATree*>(GetWorld()->AddActor(std::make_unique<ATree>(pos, kind)));
}

ATree::ATree(const FVector& pos, TreeKind kind) {
    Name = "Tree";

    // Same per-actor init the C path splits between add_actor_to_empty_slot,
    // spawn_foliage, and actor_init.
    Pos[0] = pos.x * gTrackDirection;
    Pos[1] = pos.y;
    Pos[2] = pos.z;
    Rot[0] = 0x4000;
    Rot[1] = 0;
    Rot[2] = 0;
    Flags = -0x8000;
    Flags |= 0x4000;
    State = 0x43;
    BoundingBoxSize = 3.0f;
    Unk_08 = 20.0f;
    // Vanilla quirk kept on purpose: tree init bumps the shell-cleanup counter,
    // which affects when green/red shells start recycling on tree-heavy courses.
    gNumSpawnedShells += 1;

    switch (kind) {
        case TreeKind::MarioRaceway:
        default:
            ResourceName = "mk:tree_mario_raceway";
            mDisplaylist = d_course_mario_raceway_dl_tree;
            mTlut = common_tlut_trees_import;
            mDrawDistance = 16000000.0f;
            mShadowDistance = 250000.0f;
            mShadowScale = 3.0f;
            break;
    }
    Model = mDisplaylist;

    _idx = _count;
    _count += 1;
}

bool ATree::IsMod() {
    return true;
}

void ATree::Tick() {
    // Parity with update_actor_static_plant().
    if (((Flags & 0x800) == 0) && ((Flags & 0x400) != 0)) {
        Pos[1] = Pos[1] + 4.0f;
        if (Pos[1] > 800.0f) {
            Flags |= 0x800;
        }
    }
}

void ATree::Draw(Camera* camera) {
    Mat4 mtx;
    f32 dist;

    if ((Flags & 0x800) != 0) {
        return;
    }

    dist = is_within_render_distance(camera->pos, Pos, camera->rot[1], 0, camera->fieldOfView, mDrawDistance);
    if (CVarGetInteger("gNoCulling", 0) == 1) {
        dist = MAX(dist, 0.0f);
    }
    if (dist < 0.0f) {
        return;
    }

    FrameInterpolation_RecordOpenChild("mk:tree", TAG_OBJECT((_idx << 4) | camera->cameraId));

    if (((Flags & 0x400) == 0) && (dist < mShadowDistance)) {
        func_8029794C(Pos, Rot, mShadowScale);
    }

    // Camera-facing billboard, same math render_course_actors() preloads into
    // sBillBoardMtx for the C trees; the mod-actor draw pass has no shared setup.
    f32 s = sins(camera->rot[1] - 0x8000);
    f32 c = coss(camera->rot[1] - 0x8000);
    mtx[0][0] = c;
    mtx[0][1] = 0.0f;
    mtx[0][2] = -s;
    mtx[0][3] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = 1.0f;
    mtx[1][2] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][0] = s;
    mtx[2][1] = 0.0f;
    mtx[2][2] = c;
    mtx[2][3] = 0.0f;
    mtx[3][0] = Pos[0];
    mtx[3][1] = Pos[1];
    mtx[3][2] = Pos[2];
    mtx[3][3] = 1.0f;

    // The C actor pass emits this state once for the whole list; here each tree
    // sets it because the mod-actor pass carries no shared preamble.
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
    gSPSetLights1(gDisplayListHead++, D_800DC610[1]);
    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    if (render_set_position(mtx, 0) != 0) {
        if (mTlut != nullptr) {
            gDPLoadTLUT_pal256(gDisplayListHead++, mTlut);
        }
        gSPDisplayList(gDisplayListHead++, (Gfx*) mDisplaylist);
    }

    FrameInterpolation_RecordCloseChild();
}

void ATree::Collision(Player* player, UNUSED AActor* actor) {
    // Same Boo-ghost exemption the C collision switch applies to trees.
    if (!(player->effects & BOO_EFFECT)) {
        collision_tree(player, GetWorld()->ConvertAActorToActor(this));
    }
}

void ATree::Destroy() {
}
