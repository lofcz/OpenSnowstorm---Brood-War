#ifndef BWGAME_SERIALIZATION_H
#define BWGAME_SERIALIZATION_H

#include "bwgame_state.h"
#include "actions.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <type_traits>

namespace bwgame {

namespace serialization {

static constexpr uint32_t OSV_MAGIC = 0x534e4f57; // 'SNOW'
static constexpr uint32_t OSV_VERSION = 9;

struct state_serializer {
  state &st;

  template <typename T> void write_pod(std::ostream &os, const T &val) { os.write((const char *)&val, sizeof(T)); }
  template <typename T> void read_pod(std::istream &is, T &val) { is.read((char *)&val, sizeof(T)); }

  uint32_t to_idx(const void *ptr) {
    if (!ptr) return 0xffffffff;
    return (uint32_t)((const image_t*)ptr)->index; 
  }

  template<typename T, typename C>
  T* from_idx(uint32_t idx, C& cont) {
    if (idx == 0xffffffff) return nullptr;
    return const_cast<T*>(cont.at((size_t)idx));
  }

  template<typename T>
  uint32_t type_id(const T* ptr) { return ptr ? (uint32_t)ptr->id : 0xffff; }

  template<typename T, typename V>
  T* id_to_type(uint32_t id, V& vec) { 
    if (id == 0xffff) return nullptr;
    return &vec[(size_t)id];
  }

  // -------------------------------------------------------------------------
  // State Functions Stubs
  // -------------------------------------------------------------------------
  bool unit_is_carrier(unit_t* u) { return (size_t)u->unit_type->id == 72 || (size_t)u->unit_type->id == 82; }
  bool unit_is_reaver(unit_t* u) { return (size_t)u->unit_type->id == 83 || (size_t)u->unit_type->id == 81; }
  bool ut_resource(unit_t* u) { return (size_t)u->unit_type->id >= 176 && (size_t)u->unit_type->id <= 178; }

  // -------------------------------------------------------------------------
  // CORE SAVE
  // -------------------------------------------------------------------------

  void save_state(std::ostream &os) {
    size_t offset = offsetof(state_base_copyable, update_tiles_countdown);
    size_t size = sizeof(state_base_copyable) - offset;
    os.write((const char *)&st + offset, (std::streamsize)size);

    auto write_vec = [&](auto &v) {
      size_t sz = v.size(); write_pod(os, sz);
      if (sz) os.write((const char *)v.data(), (std::streamsize)(sz * sizeof(v[0])));
    };
    write_vec(st.tiles);
    write_vec(st.tiles_mega_tile_index);
    write_vec(st.repulse_field);

    auto save_pool = [&](auto &cont, auto&& fixup) {
      write_pod(os, (uint32_t)cont.size);
      size_t n = 0;
      using T = std::remove_cv_t<std::remove_reference_t<decltype(*cont.at(0))>>;
      std::vector<uint8_t> buffer(sizeof(T));
      for (auto &arr : cont.list) {
        size_t to_write = std::min(arr.size(), cont.size - n);
        for (size_t i = 0; i < to_write; ++i) {
          memcpy(buffer.data(), &arr[i], sizeof(T));
          fixup(*(T*)buffer.data());
          os.write((const char *)buffer.data(), (std::streamsize)sizeof(T));
        }
        n += to_write;
      }
    };

    save_pool(st.units_container, [&](unit_t &u) {
      u.sprite = (sprite_t*)(size_t)to_idx(u.sprite);
      u.unit_type = (const unit_type_t*)(size_t)type_id(u.unit_type);
      u.order_type = (const order_type_t*)(size_t)type_id(u.order_type);
      u.order_unit_type = (const unit_type_t*)(size_t)type_id(u.order_unit_type);
      u.previous_unit_type = (const unit_type_t*)(size_t)type_id(u.previous_unit_type);
      u.order_target.unit = (unit_t*)(size_t)to_idx(u.order_target.unit);
      u.subunit = (unit_t*)(size_t)to_idx(u.subunit);
      u.auto_target_unit = (unit_t*)(size_t)to_idx(u.auto_target_unit);
      u.connected_unit = (unit_t*)(size_t)to_idx(u.connected_unit);
      u.worker.powerup = (unit_t*)(size_t)to_idx(u.worker.powerup);
      u.worker.target_resource_unit = (unit_t*)(size_t)to_idx(u.worker.target_resource_unit);
      u.worker.gather_target = (unit_t*)(size_t)to_idx(u.worker.gather_target);
      u.building.addon = (unit_t*)(size_t)to_idx(u.building.addon);
      u.building.rally.unit = (unit_t*)(size_t)to_idx(u.building.rally.unit);
      u.building.nydus.exit = (unit_t*)(size_t)to_idx(u.building.nydus.exit);
      u.building.silo.nuke = (unit_t*)(size_t)to_idx(u.building.silo.nuke);
      u.current_build_unit = (unit_t*)(size_t)to_idx(u.current_build_unit);
      u.irradiated_by = (unit_t*)(size_t)to_idx(u.irradiated_by);
      u.flingy_type = (const flingy_type_t*)(size_t)type_id(u.flingy_type);
      u.building.researching_type = (const tech_type_t*)(size_t)type_id(u.building.researching_type);
      u.building.upgrading_type = (const upgrade_type_t*)(size_t)type_id(u.building.upgrading_type);
      u.building.addon_build_type = (const unit_type_t*)(size_t)type_id(u.building.addon_build_type);
      u.move_target.unit = (unit_t*)(size_t)to_idx(u.move_target.unit);
      u.path = nullptr; 
      if (u.unit_type && (size_t)u.unit_type->id == 150) u.ghost.nuke_dot = nullptr;
    });

    save_pool(st.bullets_container, [&](bullet_t &b) {
      b.sprite = (sprite_t*)(size_t)to_idx(b.sprite);
      b.weapon_type = (const weapon_type_t*)(size_t)type_id(b.weapon_type);
      b.bullet_target = (unit_t*)(size_t)to_idx(b.bullet_target);
      b.bullet_owner_unit = (unit_t*)(size_t)to_idx(b.bullet_owner_unit);
      b.prev_bounce_unit = (unit_t*)(size_t)to_idx(b.prev_bounce_unit);
      b.flingy_type = (const flingy_type_t*)(size_t)type_id(b.flingy_type);
    });

    save_pool(st.sprites_container, [&](sprite_t &s) {
      s.main_image = (image_t*)(size_t)to_idx(s.main_image);
      s.sprite_type = (const sprite_type_t*)(size_t)type_id(s.sprite_type);
    });

    save_pool(st.images_container, [&](image_t &i) {
      i.sprite = (sprite_t*)(size_t)to_idx(i.sprite);
      i.image_type = (const image_type_t*)(size_t)type_id(i.image_type);
    });

    save_pool(st.orders_container, [&](order_t &o) {
      o.order_type = (const order_type_t*)(size_t)type_id(o.order_type);
      o.target.unit = (unit_t*)(size_t)to_idx(o.target.unit);
    });

    auto save_list_seq = [&](auto &l) {
        size_t n = l.size(); write_pod(os, n);
        for (auto &v : l) write_pod(os, v);
    };
    save_list_seq(st.paths);
    save_list_seq(st.thingies);

    auto save_list = [&](auto &list) {
      size_t n = 0; for (auto* v : ptr(list)) ++n;
      write_pod(os, n);
      for (auto* v : ptr(list)) write_pod(os, to_idx(v));
    };
    save_list(st.visible_units);
    save_list(st.hidden_units);
    save_list(st.map_revealer_units);
    save_list(st.dead_units);
    for (int i = 0; i < 12; ++i) save_list(st.player_units[i]);
    save_list(st.cloaked_units);
    save_list(st.psionic_matrix_units);
    save_list(st.active_bullets);
    
    save_list(st.units_container.free_list);
    save_list(st.bullets_container.free_list);
    save_list(st.sprites_container.free_list);
    save_list(st.images_container.free_list);
    save_list(st.orders_container.free_list);

    for (size_t i = 0; i < st.units_container.size; ++i) save_list(st.units_container.at(i)->order_queue);
    for (size_t i = 0; i < st.sprites_container.size; ++i) save_list(st.sprites_container.at(i)->images);

    auto write_finder = [&](auto &v) {
      size_t sz = v.size(); write_pod(os, sz);
      for (auto &e : v) { write_pod(os, to_idx(e.u)); write_pod(os, e.value); }
    };
    write_finder(st.unit_finder_x);
    write_finder(st.unit_finder_y);

    // AI scripts
    for (int p = 0; p < 8; ++p) {
      size_t n = st.ai_st.player_scripts[p].size();
      write_pod(os, (uint32_t)n);
      for (const auto &s : st.ai_st.player_scripts[p]) {
        ai_script_t tmp = s;
        tmp.center_unit = (unit_t*)(size_t)to_idx(s.center_unit);
        write_pod(os, tmp);
      }
    }
  }

  void load_state(std::istream &is, const global_state *g, game_state *gm) {
    size_t offset = offsetof(state_base_copyable, update_tiles_countdown);
    size_t size = sizeof(state_base_copyable) - offset;
    is.read((char *)&st + offset, (std::streamsize)size);
    st.global = g; st.game = gm;

    auto read_vec = [&](auto &v) {
      size_t sz; read_pod(is, sz); v.resize(sz);
      if (sz) is.read((char *)v.data(), (std::streamsize)(sz * sizeof(v[0])));
    };
    read_vec(st.tiles);
    read_vec(st.tiles_mega_tile_index);
    read_vec(st.repulse_field);

    auto load_pool = [&](auto &cont) {
      size_t sz; read_pod(is, sz);
      cont.list.clear(); cont.size = 0; cont.free_list.clear();
      using T = std::remove_cv_t<std::remove_reference_t<decltype(*cont.at(0))>>;
      while (cont.size < (size_t)sz) {
        cont.grow(false);
        auto &arr = cont.list.back();
        size_t to_read = std::min(arr.size(), (size_t)sz - (cont.size - arr.size()));
        is.read((char *)arr.data(), (std::streamsize)(to_read * sizeof(T)));
      }
    };
    load_pool(st.units_container);
    load_pool(st.bullets_container);
    load_pool(st.sprites_container);
    load_pool(st.images_container);
    load_pool(st.orders_container);

    auto id_to_u = [&](uint32_t id) { return from_idx<unit_t>(id, st.units_container); };
    auto id_to_s = [&](uint32_t id) { return from_idx<sprite_t>(id, st.sprites_container); };
    auto id_to_i = [&](uint32_t id) { return from_idx<image_t>(id, st.images_container); };

    for (size_t i = 0; i < st.units_container.size; ++i) {
      unit_t &u = *st.units_container.at(i);
      u.sprite = id_to_s((uint32_t)(size_t)u.sprite);
      u.unit_type = id_to_type<const unit_type_t>((uint32_t)(size_t)u.unit_type, st.game->unit_types.vec);
      u.order_type = id_to_type<const order_type_t>((uint32_t)(size_t)u.order_type, st.global->order_types.vec);
      u.order_unit_type = id_to_type<const unit_type_t>((uint32_t)(size_t)u.order_unit_type, st.game->unit_types.vec);
      u.previous_unit_type = id_to_type<const unit_type_t>((uint32_t)(size_t)u.previous_unit_type, st.game->unit_types.vec);
      u.order_target.unit = id_to_u((uint32_t)(size_t)u.order_target.unit);
      u.subunit = id_to_u((uint32_t)(size_t)u.subunit);
      u.auto_target_unit = id_to_u((uint32_t)(size_t)u.auto_target_unit);
      u.connected_unit = id_to_u((uint32_t)(size_t)u.connected_unit);
      u.worker.powerup = id_to_u((uint32_t)(size_t)u.worker.powerup);
      u.worker.target_resource_unit = id_to_u((uint32_t)(size_t)u.worker.target_resource_unit);
      u.worker.gather_target = id_to_u((uint32_t)(size_t)u.worker.gather_target);
      u.building.addon = id_to_u((uint32_t)(size_t)u.building.addon);
      u.building.rally.unit = id_to_u((uint32_t)(size_t)u.building.rally.unit);
      u.building.nydus.exit = id_to_u((uint32_t)(size_t)u.building.nydus.exit);
      u.building.silo.nuke = id_to_u((uint32_t)(size_t)u.building.silo.nuke);
      u.current_build_unit = id_to_u((uint32_t)(size_t)u.current_build_unit);
      u.irradiated_by = id_to_u((uint32_t)(size_t)u.irradiated_by);
      u.flingy_type = id_to_type<const flingy_type_t>((uint32_t)(size_t)u.flingy_type, st.global->flingy_types.vec);
      u.building.researching_type = id_to_type<const tech_type_t>((uint32_t)(size_t)u.building.researching_type, st.game->tech_types.vec);
      u.building.upgrading_type = id_to_type<const upgrade_type_t>((uint32_t)(size_t)u.building.upgrading_type, st.game->upgrade_types.vec);
      u.building.addon_build_type = id_to_type<const unit_type_t>((uint32_t)(size_t)u.building.addon_build_type, st.game->unit_types.vec);
      u.move_target.unit = id_to_u((uint32_t)(size_t)u.move_target.unit);
    }
    for (size_t i = 0; i < st.bullets_container.size; ++i) {
      bullet_t &b = *st.bullets_container.at(i);
      b.sprite = id_to_s((uint32_t)(size_t)b.sprite);
      b.weapon_type = id_to_type<const weapon_type_t>((uint32_t)(size_t)b.weapon_type, st.game->weapon_types.vec);
      b.bullet_target = id_to_u((uint32_t)(size_t)b.bullet_target);
      b.bullet_owner_unit = id_to_u((uint32_t)(size_t)b.bullet_owner_unit);
      b.prev_bounce_unit = id_to_u((uint32_t)(size_t)b.prev_bounce_unit);
      b.flingy_type = id_to_type<const flingy_type_t>((uint32_t)(size_t)b.flingy_type, st.global->flingy_types.vec);
    }
    for (size_t i = 0; i < st.sprites_container.size; ++i) {
      sprite_t &s = *st.sprites_container.at(i);
      s.main_image = id_to_i((uint32_t)(size_t)s.main_image);
      s.sprite_type = id_to_type<const sprite_type_t>((uint32_t)(size_t)s.sprite_type, st.global->sprite_types.vec);
    }
    for (size_t i = 0; i < st.images_container.size; ++i) {
      image_t &im = *st.images_container.at(i);
      im.sprite = id_to_s((uint32_t)(size_t)im.sprite);
      im.image_type = id_to_type<const image_type_t>((uint32_t)(size_t)im.image_type, st.global->image_types.vec);
    }
    for (size_t i = 0; i < st.orders_container.size; ++i) {
      order_t &o = *st.orders_container.at(i);
      o.order_type = id_to_type<const order_type_t>((uint32_t)(size_t)o.order_type, st.global->order_types.vec);
      o.target.unit = id_to_u((uint32_t)(size_t)o.target.unit);
    }

    auto load_list_seq = [&](auto &l) {
        size_t n; read_pod(is, n); l.clear();
        for (size_t i = 0; i < n; ++i) {
            typename std::remove_cv_t<std::remove_reference_t<decltype(l.front())>> v{};
            read_pod(is, v);
            l.push_back(v);
        }
    };
    load_list_seq(st.paths);
    load_list_seq(st.thingies);

    auto load_list = [&](auto &list, auto &cont) {
      size_t n; read_pod(is, n); list.clear();
      for (size_t i = 0; i < n; ++i) {
        uint32_t idx; read_pod(is, idx);
        list.push_back(*const_cast<typename std::remove_reference<decltype(*list.begin())>::type*>(cont.at((size_t)idx)));
      }
    };
    load_list(st.visible_units, st.units_container);
    load_list(st.hidden_units, st.units_container);
    load_list(st.map_revealer_units, st.units_container);
    load_list(st.dead_units, st.units_container);
    for (int i = 0; i < 12; ++i) load_list(st.player_units[i], st.units_container);
    load_list(st.cloaked_units, st.units_container);
    load_list(st.psionic_matrix_units, st.units_container);
    load_list(st.active_bullets, st.bullets_container);
    
    load_list(st.units_container.free_list, st.units_container);
    load_list(st.bullets_container.free_list, st.bullets_container);
    load_list(st.sprites_container.free_list, st.sprites_container);
    load_list(st.images_container.free_list, st.images_container);
    load_list(st.orders_container.free_list, st.orders_container);

    for (size_t i = 0; i < st.units_container.size; ++i) load_list(st.units_container.at(i)->order_queue, st.orders_container);
    for (size_t i = 0; i < st.sprites_container.size; ++i) load_list(st.sprites_container.at(i)->images, st.images_container);

    auto read_finder = [&](auto &v) {
      size_t sz; read_pod(is, sz); v.resize(sz);
      for (auto &e : v) { uint32_t idx; read_pod(is, idx); e.u = id_to_u(idx); read_pod(is, e.value); }
    };
    read_finder(st.unit_finder_x);
    read_finder(st.unit_finder_y);

    // AI scripts
    for (int p = 0; p < 8; ++p) {
      uint32_t n; read_pod(is, n); st.ai_st.player_scripts[p].clear();
      for (uint32_t i = 0; i < n; ++i) {
        ai_script_t s;
        read_pod(is, s);
        s.center_unit = id_to_u((uint32_t)(size_t)s.center_unit);
        st.ai_st.player_scripts[p].push_back(std::move(s));
      }
    }
  }

  void save_action_state(std::ostream &os, const action_state &as) {
    for (auto v : as.player_id) write_pod(os, v);
    write_pod(os, as.actions_data_position);
    write_pod(os, as.next_action_frame);
    for (auto &sel : as.selection) {
      size_t n = sel.size(); write_pod(os, (uint32_t)n);
      for (auto* u : sel) write_pod(os, to_idx(u));
    }
    for (auto &group_p : as.control_groups) {
      for (auto &group : group_p) {
        size_t n = group.size(); write_pod(os, (uint32_t)n);
        for (auto id : group) write_pod(os, (uint32_t)id.raw_value);
      }
    }
  }

  void load_action_state(std::istream &is, action_state &as) {
    for (auto &v : as.player_id) read_pod(is, v);
    read_pod(is, as.actions_data_position);
    read_pod(is, as.next_action_frame);
    for (auto &sel : as.selection) {
      uint32_t n; read_pod(is, n); sel.clear();
      for (size_t i = 0; i < (size_t)n; ++i) {
        uint32_t idx; read_pod(is, idx);
        sel.push_back(from_idx<unit_t>(idx, st.units_container));
      }
    }
    for (auto &group_p : as.control_groups) {
      for (auto &group : group_p) {
        uint32_t n; read_pod(is, n); group.clear();
        for (size_t i = 0; i < (size_t)n; ++i) {
          uint32_t raw; read_pod(is, raw);
          group.push_back(unit_id{ (uint16_t)raw });
        }
      }
    }
  }

  void save_full(const a_string &filename, const action_state &as, const std::array<apm_t, 12> &apm) {
    std::ofstream os(filename.c_str(), std::ios::binary);
    if (!os) return;
    write_pod(os, OSV_MAGIC);
    write_pod(os, OSV_VERSION);
    save_state(os);
    save_action_state(os, as);
    os.write((const char*)apm.data(), sizeof(apm));
  }

  bool load_full(const a_string &filename, action_state &as, std::array<apm_t, 12> &apm, const global_state *g, game_state *gm) {
    std::ifstream is(filename.c_str(), std::ios::binary);
    if (!is) return false;
    uint32_t magic, version;
    read_pod(is, magic); read_pod(is, version);
    if (magic != OSV_MAGIC) return false;
    load_state(is, g, gm);
    load_action_state(is, as);
    is.read((char*)apm.data(), sizeof(apm));
    return true;
  }
};

} // namespace serialization

} // namespace bwgame

#endif
