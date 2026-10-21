# Moddable composite worlds (experimental)

This is a reusable **client-side presentation layer** that can display
independently persisted server worlds in a shared virtual coordinate space.
It deliberately does **not** implement hardcoded ship landing or planet
selection. Mods own the composition lifecycle.

## Architecture

- `UniverseClient` stores per-world virtual placements. Coordinates
  are translated only for presentation: `virtual = local + placement.offset`.
- Each loaded, eligible SubWorld can request a tile window and generate
  its own `WorldRenderData`, without owning the player.
- The client draws each source through an **independent** `WorldPainter`,
  with its source geometry and a camera translated into source coordinates.
  The painter's scissor region is limited to the source's complete vertical
  extent. Its original sky, planetary space biome, terrain, and buildings
  remain in its own world.
- Secondary worlds outside the viewport have an empty tile window, but keep
  their subscription while a placement is registered. Removing a placement
  releases that keepalive; the mod may then unload the SubWorld.
- No world files, simulation coordinates, or persisted worlds are merged.

The first compositor supports **vertical stacking**, not overlapping worlds
with custom z priorities or independently clipped horizontal regions.

## Mod-facing Lua calls (main client thread)

```lua
local primary = universe.playerWorldId()
universe.loadSubWorld(planetWorldId) -- asynchronous / may be rejected

local planetSize = universe.compositeWorldSize(planetWorldId)
if planetSize then
  universe.setCompositeWorldPlacement(planetWorldId, {0, 0})
  universe.setCompositeWorldPlacement(primary, {0, planetSize[2] + 8})
end

-- Only once the mounted world is safely out of frame:
universe.clearCompositeWorldPlacement(planetWorldId)
universe.clearCompositeWorldPlacement(primary)
universe.unloadSubWorld(planetWorldId)
```

`subWorldActive()` may return true for a **requested** world. Use
`compositeWorldSize()` to check whether the WorldClient actually exists
and has received its world template. The example chooses a gap of eight
tiles; the mod should make it configurable.

### Queries used when preparing a seamless handoff

`universe.compositeWorldSize(worldId)` returns `nil` until the target world
is initialized and its actual dimensions are known.

`universe.compositeRegionAtHeight(virtualY)` returns the single loaded,
registered world whose original vertical bounds contain the requested
coordinate. If two worlds overlap or a required world is unavailable, it
returns `nil` rather than silently assigning authority. This function
does **not** transfer players; the authoritative handoff protocol remains
unimplemented.

The compositor clips each source's tile subscription to its original
`[0, height)` vertical tile interval while painting the screen's
intersection with that region. A world may remain subscribed with an
empty tile viewport while out of frame.

## Server permission for planetary test mods

SubWorld subscriptions still use OpenStarbound's existing server
authorization. A new, optional setting provides a narrower alternative to
granting access to every world:

```json
{
  "allowClientSubWorldsCurrentOrbit": true
}
```

This setting is **false by default** and is evaluated on the authoritative
server. It permits a player **aboard their own ship** to subscribe to exactly
the celestial body at the ship's current location/orbit. It does not permit
arbitrary remote planets, other people's ships or arbitrary instances.
`disallowClientSubWorlds` still takes precedence.

Enable it only on a server you control and trust; SubWorld clients can run
scripts and are not automatically restricted to read-only behavior.

## Remaining engine work

1. **Gameplay transfers**: the server still owns the player in one
   authoritative world. Stepping across a rendered seam does not currently
   transfer the player or preserve velocity.
2. **Uninterrupted camera handoff**: a player's primary world changing still
   needs camera-coordinate continuity and a handoff acknowledgement.
3. **Advanced compositor**: per-region sorting, horizontal clipping and
   genuinely overlapping visual regions need explicit policies.
4. **Runtime validation**: C++ compilation, multiplayer security, GPU
   renderer behavior, lighting, planet wrap, world startup latency and
   rapid attach/detach must be tested before releasing a non-draft PR.

Any transition API must retain **server authority** and avoid letting an
untrusted client simply overwrite its world membership or physics state.
