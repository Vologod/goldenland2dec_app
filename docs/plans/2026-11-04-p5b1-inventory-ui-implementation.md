# P5b1 Read-only Inventory UI — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Показать read-only инвентарь из выбранного save в оригинальном 1024×768 UI: bottom strip (container 1), hot slots (container 0), puppet (container 2), filters/scroll/right panel.

**Architecture:** `assets` парсит 3 контейнера и item catalog; `game` — filter/page/selection; `app` — SDL overlay с letterbox; `glgame` — открытие/вход и выбранный save.

**Tech Stack:** C++20, miniz, doctest, SDL2, существующие `GameVfs`, `parse_item`, `Sdb`, `Font`.

## Global Constraints

- Client.dll SHA-256 `FBF213B4B820E767A529556D0F7E4D29EE2202E7E39C3E49043F49D55DAB5C16`.
- UI-холст 1024×768, letterbox 4:3; hit-test в логических координатах.
- Container 0 = hot slots (mask `(record+0x0C & 0x0000F000)>>12`, 1..9); container 1 = bottom strip 12 slots `y=703`; container 2 = puppet.
- Bottom x: `140,201,261,321,382,443,504,565,627,688,749,811`; slot 55×55.
- Hot slots: object 99 `(10,618,266×40)`, y=624, x `17,14,43,72,101,130,159,188,217,246`, slots 1..9, target 27×27.
- Puppet: 55×55; masks/coords: `0x004000(67,307)`, `0x008000(168,308)`,
  `0x010000(0,354)`, `0x020000(233,354)`, `0x040000(4,402)`, `0x080000(229,402)`,
  `0x001000(22,472)`, `0x400000(216,472)`, `0x002000(16,466)`, `0x800000(211,466)`,
  `0x100000(16,547)`, `0x200000(216,547)`.
- Filters exclusive: id9..15 → masks `1,2,4,8,0x10,0x20,0`; type 18 always visible;
  filter groups per research §7. Scroll page stride 12, arrows ±1, wheel ±3.
- Icons native size, centered `off=max(0,(target-tex)/2)`; no scaling; path normalized
  lower-case `items/res/<icon_name>`; fallback `engineres/interface/bag`.
- Right panel is port decision D-045: outer `(654,44,360,520)`, content
  `(664,54,340,500)`, wrap/scroll rules per research §6a.
- No use/drop/equip/save mutation; no game assets committed; no comments.

---

### Task 1: `HeroInventory` parser (3 containers)

**Files:**
- Modify `src/assets/include/gl/assets/hero_equipment.hpp`, `src/assets/src/hero_equipment.cpp`
- Test `tests/assets/test_hero_equipment.cpp`, `tests/assets/test_hero_equipment_real.cpp`

**Interfaces:**
- Produces:
```cpp
struct InventoryItem { std::uint32_t item_id, count, flags, type, style; };
struct HeroInventory {
    std::vector<InventoryItem> hot, items, equipped;
};
Result<HeroInventory> parse_hero_inventory(std::span<const std::uint8_t> body);
```
- Preserve `parse_hero_equipment` (derive equipped) for P5a.

- [x] **Step 1: Failing test counts/IDs**
```cpp
auto body = gl::assets::decode_save_body(file_bytes);
auto inv = gl::assets::parse_hero_inventory(*body);
REQUIRE(inv.has_value());
CHECK(inv->hot.size() == 8);
CHECK(inv->items.size() == 117);
CHECK(inv->equipped.size() == 11);
```
Real slot0/slot2 counts `8/117/11` and `9/140/11`.

- [x] **Step 2: Run, expect fail** `cmake --build build --target gl_assets_tests gl_assets_real_tests`.

- [x] **Step 3: Implement** parse all 3 containers with existing record formula; count
  from `+0xB0`; clamp to 1 for equipped; fill item fields.

- [x] **Step 4: Run + commit** `gl_assets_tests`/`gl_assets_real_tests`; commit
  `feat(assets): parse all BT_HERO containers`.

---

### Task 2: `ItemCatalog` (ITM/SDB/icon)

**Files:**
- Create `src/assets/include/gl/assets/item_catalog.hpp`, `src/assets/src/item_catalog.cpp`
- Modify `src/assets/CMakeLists.txt`
- Test `tests/assets/test_item_catalog.cpp`, real `test_item_catalog_real.cpp`

**Interfaces:**
```cpp
struct ItemCatalogEntry {
    gl::assets::Item item;
    std::string name, description, icon_path;
};
class ItemCatalog {
  public:
    static Result<ItemCatalog> load(const GameVfs&);
    const ItemCatalogEntry* find(std::uint32_t item_id) const;
};
```

- [x] **Step 1: Failing real test** slot0 item 318: name `Алмазные латы`,
  icon `items/res/t_armour/almaz/00.bmp`, price 22294, weight 98.
- [x] **Step 2: Implement** load `lit_names.sdb`/`descriptions.sdb`; lazy ITM
  `items/data/<id>.itm`; icon path normalize `\`→`/` lower-case; fallback
  `engineres/interface/bag`.
- [x] **Step 3: Run + commit** `feat(assets): add item catalog with names/icons`.

---

### Task 3: `InventoryView` filters/pages/selection

**Files:**
- Create `src/game/include/gl/game/inventory_view.hpp`, `src/game/src/inventory_view.cpp`
- Modify `src/game/CMakeLists.txt`
- Test `tests/game/test_inventory_view.cpp`

**Interfaces:**
```cpp
struct InventoryViewState {
    std::uint32_t filter_mask = 0;
    std::size_t page = 0;
    std::optional<std::size_t> selected;
};
std::vector<std::size_t> visible_inventory(const HeroInventory&, std::uint32_t mask);
std::size_t clamp_page(std::size_t page, std::size_t count);
```

- [x] **Step 1: Failing filter tests** type 18 always visible; mask 1 → types
  0..7/28; mask 2 → 11..14; mask 4 → 15..17; mask 8 → 21/22; mask 0x10 →
  8/9/10/19/20/23/24/26/27; mask 0x20 → 25; mask 0 → all.
- [x] **Step 2: Implement** exact groups; page clamp `0..max(0,count-12)`;
  filter change resets page 0.
- [x] **Step 3: Run + commit** `feat(game): add inventory filter/page view`.

---

### Task 4: `InventoryOverlay` rendering + input

**Files:**
- Create `src/app/include/gl/app/inventory_overlay.hpp`, `src/app/src/inventory_overlay.cpp`
- Modify `src/app/CMakeLists.txt`
- Test `tests/app/test_inventory_overlay.cpp` (synthetic catalog/inventory, screenshot)

**Interfaces:**
```cpp
class InventoryOverlay {
  public:
    bool open(const GameVfs&, const ItemCatalog&, const HeroInventory&);
    void close();
    bool visible() const;
    void render(SDL_Renderer*, int screen_w, int screen_h);
    bool handle_mouse(int x, int y, bool click);
    bool handle_key(SDL_Keycode);
};
```

- [x] **Step 1: Failing render/hit tests**
  - background 1024×768 letterboxed into 800×600 → scale 0.5859, offset y=0;
  - click slot 0 at logical `(140,703)` selects item; right panel shows name;
  - filter click object9 sets mask 1; scroll next page; exit closes.
- [x] **Step 2: Implement** load `inventory_secondary`/`inventory_skills`,
  filters/exit/scroll/drop textures; draw bottom strip slots/icons, hot slots,
  puppet slots, filters, right panel (D-045); native-size icons centered; stack
  count `>1` bottom-right font; skills tab disabled.
- [x] **Step 3: Run + commit** `feat(app): add read-only inventory overlay`.

---

### Task 5: glgame integration

**Files:**
- Modify `tools/glgame/main.cpp`
- Test `tests/app/test_glgame_screenshot.cpp`

**Interfaces:**
- Store `std::optional<HeroInventory> pending_inventory_` / current inventory;
  overlay owns textures and view state.
- Add `--inventory` / key `I` toggle; touch button hook later.

- [x] **Step 1: Failing screenshot test** run `--equip-save 0 --inventory --screenshot`;
  assert non-empty, slot0 name visible; `--inventory` with invalid save → empty overlay.
- [x] **Step 2: Implement** load inventory from selected save (same path as P5a),
  open overlay, route mouse/key, pause level input while overlay active.
- [x] **Step 3: Run + commit** `feat(glgame): open inventory from selected save`.

---

### Task 6: docs, full ctest, manual gate

- [x] **Step 1: Update STATUS/PROJECT/DECISIONS** with implementation, tests, D-045.
- [x] **Step 2: Full ctest** with `GL_GAME_DIR`; record count/time.
- [ ] **Step 3: Manual gate** — user compares overlay with original UI resources
  (inventory_secondary/skills) and slot0/slot2 contents; agent does not claim pass.
- [x] **Step 4: Commit docs.**

## Self-review notes

- All rects/masks/paths from `docs/research/p5b1-inventory-ui.md`; no guessed geometry.
- P5b1 read-only; no save mutation; skills/actions deferred.
- Right panel is explicit deviation D-045.
