#pragma once

#include <libultraship.h>
#include "engine/Actor.h"

extern "C" {
#include "common_structs.h"
}

// Which course's tree/foliage variant this instance is. Every stock spawn_foliage
// course is covered; the C render/update path (actors/trees/render.inc.c) still
// serves the podium ceremony's foliage, which runs on the legacy actor loop.
enum class TreeKind {
    MarioRaceway,
    YoshiValley,
    RoyalRaceway,
    MooMooFarm,
    LuigiRaceway,
    PeachCastle,
    FrappeSnowland,
    KalimariCactus1,
    KalimariCactus2,
    KalimariCactus3,
    BowserBush,
};

class ATree : public AActor {
  public:
    ATree(const FVector& pos, TreeKind kind);

    // SpawnActor<ATree>(...) helper, same pattern as the other converted actors.
    static ATree* Spawn(FVector pos, TreeKind kind);

    bool IsMod() override;
    void Tick() override;
    void Draw(Camera* camera) override;
    void Collision(Player* player, AActor* actor) override;
    void Destroy() override;

  private:
    // Per-variant render parameters, values verbatim from the C render functions.
    const char* mDisplaylist;
    const char* mTlut;      // nullptr = this variant loads no palette
    f32 mDrawDistance;      // squared-distance cull threshold
    f32 mShadowDistance;    // squared distance under which the ground shadow draws
    f32 mShadowScale;

    size_t _idx;
    static size_t _count;
};
