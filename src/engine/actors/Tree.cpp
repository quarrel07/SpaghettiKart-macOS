#include "Tree.h"

#include <macros.h>
#include <libultra/gbi.h>
#include <assets/models/tracks/mario_raceway/mario_raceway_data.h>
#include <assets/models/tracks/yoshi_valley/yoshi_valley_data.h>
#include <assets/models/tracks/royal_raceway/royal_raceway_data.h>
#include <assets/models/tracks/moo_moo_farm/moo_moo_farm_data.h>
#include <assets/models/tracks/luigi_raceway/luigi_raceway_data.h>
#include <assets/models/tracks/frappe_snowland/frappe_snowland_data.h>
#include <assets/models/tracks/kalimari_desert/kalimari_desert_data.h>
#include <assets/models/tracks/bowsers_castle/bowsers_castle_data.h>
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
#include "racing/collision.h"
#include "code_800029B0.h"
#include "courses/all_course_data.h"
}

size_t ATree::_count = 0;

namespace {
// Per-variant values, verbatim from actor_init and actors/trees/render.inc.c.
struct TreeVariant {
    const char* resourceName;
    const char* displaylist;
    const char* tlut; // nullptr = the variant loads no palette
    f32 drawDistance;
    f32 shadowDistance;
    f32 shadowScale;
    s16 state;
    f32 unk08;
    bool collidable;        // whether init sets the 0x4000 collision flag
    bool bumpsShellCounter; // Mario Raceway trees bump gNumSpawnedShells (vanilla quirk)
};

TreeVariant GetVariant(TreeKind kind) {
    switch (kind) {
        case TreeKind::MarioRaceway:
        default:
            return { "mk:tree_mario_raceway", d_course_mario_raceway_dl_tree,   common_tlut_trees_import,
                     16000000.0f, 250000.0f, 3.0f,        0x43, 20.0f, true,  true };
        case TreeKind::YoshiValley:
            return { "mk:tree_yoshi_valley",  d_course_yoshi_valley_dl_tree,    common_tlut_trees_import,
                     4000000.0f,  250000.0f, 2.79999995f, 0x43, 23.0f, true,  false };
        case TreeKind::RoyalRaceway:
            return { "mk:tree_royal_raceway", d_course_royal_raceway_dl_tree,   common_tlut_trees_import,
                     4000000.0f,  250000.0f, 2.79999995f, 0x43, 17.0f, true,  false };
        case TreeKind::MooMooFarm:
            return { "mk:tree_moo_moo_farm",  d_course_moo_moo_farm_dl_tree,    common_tlut_trees_import,
                     6250000.0f,  600.0f,    5.0f,        0x43, 17.0f, false, false };
        case TreeKind::LuigiRaceway:
            return { "mk:tree_luigi_raceway", d_course_luigi_raceway_dl_FC70,   common_tlut_trees_import,
                     4000000.0f,  250000.0f, 2.79999995f, 0x43, 17.0f, true,  false };
        case TreeKind::PeachCastle:
            return { "mk:tree_peach_castle",  d_course_royal_raceway_dl_castle_tree, common_tlut_trees_import,
                     4000000.0f,  250000.0f, 2.79999995f, 0x43, 17.0f, false, false };
        case TreeKind::FrappeSnowland:
            return { "mk:tree_frappe_snowland", d_course_frappe_snowland_dl_tree, nullptr,
                     4000000.0f,  250000.0f, 2.79999995f, 0x43, 17.0f, true,  false };
        case TreeKind::KalimariCactus1:
            return { "mk:cactus1_kalimari",   d_course_kalimari_desert_dl_cactus1, nullptr,
                     4000000.0f,  40000.0f,  1.0f,        0x19, 7.0f,  true,  false };
        case TreeKind::KalimariCactus2:
            return { "mk:cactus2_kalimari",   d_course_kalimari_desert_dl_cactus2, nullptr,
                     4000000.0f,  40000.0f,  1.0f,        0x19, 7.0f,  true,  false };
        case TreeKind::KalimariCactus3:
            return { "mk:cactus3_kalimari",   d_course_kalimari_desert_dl_cactus3, nullptr,
                     4000000.0f,  40000.0f,  0.80000001f, 0x19, 7.0f,  true,  false };
        case TreeKind::BowserBush:
            return { "mk:bush_bowsers_castle", d_course_bowsers_castle_dl_bush,  common_tlut_trees_import,
                     640000.0f,   250000.0f, 2.79999995f, 0x43, 17.0f, true,  false };
    }
}
} // namespace

ATree* ATree::Spawn(FVector pos, TreeKind kind) {
    return static_cast<ATree*>(GetWorld()->AddActor(std::make_unique<ATree>(pos, kind)));
}

ATree::ATree(const FVector& pos, TreeKind kind) {
    const TreeVariant v = GetVariant(kind);

    Name = "Tree";
    ResourceName = v.resourceName;

    // Same per-actor init the C path splits between add_actor_to_empty_slot,
    // spawn_foliage, and actor_init.
    Pos[0] = pos.x * gTrackDirection;
    Pos[1] = pos.y;
    Pos[2] = pos.z;
    Rot[0] = 0x4000;
    Rot[1] = 0;
    Rot[2] = 0;
    Flags = -0x8000;
    if (v.collidable) {
        Flags |= 0x4000;
    }
    State = v.state;
    BoundingBoxSize = 3.0f;
    Unk_08 = v.unk08;
    if (v.bumpsShellCounter) {
        // Vanilla quirk kept on purpose: Mario Raceway tree init bumps the
        // shell-cleanup counter, which affects when shells start recycling.
        gNumSpawnedShells += 1;
    }

    mDisplaylist = v.displaylist;
    mTlut = v.tlut;
    mDrawDistance = v.drawDistance;
    mShadowDistance = v.shadowDistance;
    mShadowScale = v.shadowScale;
    Model = mDisplaylist;

    // spawn_foliage's ground fit: drop the tree onto the course mesh when the
    // spawn point floats above it, then lean it to the surface.
    check_bounding_collision(&Unk30, 5.0f, Pos[0], Pos[1], Pos[2]);
    if (Unk30.surfaceDistance[2] < 0.0f) {
        Pos[1] = calculate_surface_height(Pos[0], Pos[1], Pos[2], Unk30.meshIndexZX);
    }
    func_802976EC(&Unk30, Rot);

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
