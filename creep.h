#ifndef BWGAME_CREEP_H
#define BWGAME_CREEP_H



	bool tile_can_have_creep(xy_t<size_t> tile_pos) {
		size_t index = tile_pos.y * game_st.map_tile_width + tile_pos.x;
		if (st.tiles[index].flags & (tile_t::flag_unbuildable | tile_t::flag_partially_walkable)) return false;
		if (tile_pos.y == game_st.map_tile_height - 1) return true;
		if (st.tiles[index + game_st.map_tile_width].flags & tile_t::flag_unbuildable) return false;
		return true;
	}

	rect_t<xy_t<size_t>> get_max_creep_bb(unit_type_autocast unit_type, xy pos, bool unit_is_completed) {
		rect r;
		if (unit_type_spreads_creep(unit_type, unit_is_completed)) {
			r.from = pos - xy(320, 200);
			r.to = pos + xy(320, 200);
		} else {
			r.from = (pos / 32 - unit_type->placement_size / 32 / 2) * 32;
			r.to = (r.from / 32 + unit_type->placement_size / 32 - xy(1, 1)) * 32;
		}
		rect_t<xy_t<size_t>> rt;
		if (r.from.x <= 0) rt.from.x = 0;
		else rt.from.x = r.from.x / 32u;
		if (r.from.y <= 0) rt.from.y = 0;
		else rt.from.y = r.from.y / 32u;
		if (r.to.x >= (int)game_st.map_width) rt.to.x = game_st.map_tile_width - 1;
		else rt.to.x = r.to.x / 32u;
		if (r.to.y >= (int)game_st.map_height) rt.to.y = game_st.map_tile_height - 1;
		else rt.to.y = r.to.y / 32u;
		return rt;
	}

	size_t count_neighboring_creep_tiles(xy_t<size_t> tile_pos) {
		size_t r = 0;
		size_t width = game_st.map_tile_width;
		size_t index = tile_pos.y * width + tile_pos.x;
		index -= width;
		index -= 1;
		auto test = [&]() {
			if (st.tiles[index].flags & tile_t::flag_has_creep) ++r;
		};
		if (tile_pos.y != 0) {
			if (tile_pos.x != 0) test();
			++index;
			test();
			++index;
			if (tile_pos.x != width - 1) test();
			index -= 2;
		}
		index += width;
		if (tile_pos.x != 0) test();
		index += 2;
		if (tile_pos.x != width - 1) test();
		index -= 2;
		index += width;
		if (tile_pos.y != game_st.map_tile_height - 1) {
			if (tile_pos.x != 0) test();
			++index;
			test();
			++index;
			if (tile_pos.x != width - 1) test();
		}
		return r;
	}

	void set_tile_creep(xy_t<size_t> tile_pos, bool has_creep = true) {
		size_t index = tile_pos.y * game_st.map_tile_width + tile_pos.x;
		if (has_creep) st.tiles[index].flags |= tile_t::flag_has_creep;
		else st.tiles[index].flags &= ~tile_t::flag_has_creep;

		size_t width = game_st.map_tile_width;
		size_t height = game_st.map_tile_height;
		index -= width;
		--tile_pos.x;
		index -= 1;
		--tile_pos.y;
		auto test = [&]() {
			if (~st.tiles[index].flags & tile_t::flag_has_creep) return;
			if (~st.tiles[index].flags & tile_t::flag_creep_receding) return;
			auto* v = st.creep_life.table.find(tile_pos);
			if (!v) error("set_tile_creep: receding creep not found");
			size_t n_neighbors = count_neighboring_creep_tiles(tile_pos);
			if (v->n_neighboring_creep_tiles == n_neighbors) return;

			st.creep_life.lists[v->n_neighboring_creep_tiles].remove(*v);
			--st.creep_life.lists_size[v->n_neighboring_creep_tiles];
			v->n_neighboring_creep_tiles = n_neighbors;
			st.creep_life.lists[n_neighbors].push_front(*v);
			++st.creep_life.lists_size[n_neighbors];
		};
		if (tile_pos.y < height) {
			if (tile_pos.x < width) test();
			++index;
			++tile_pos.x;
			test();
			++index;
			++tile_pos.x;
			if (tile_pos.x < width) test();
			index -= 2;
			tile_pos.x -= 2;
		}
		index += width;
		++tile_pos.y;
		if (tile_pos.x < width) test();
		index += 2;
		tile_pos.x += 2;
		if (tile_pos.x < width) test();
		index -= 2;
		tile_pos.x -= 2;
		index += width;
		++tile_pos.y;
		if (tile_pos.y < height) {
			if (tile_pos.x < width) test();
			++index;
			++tile_pos.x;
			test();
			++index;
			++tile_pos.x;
			if (tile_pos.x < width) test();
		}
	}

	bool spread_creep(unit_type_autocast unit_type, xy pos, bool* out_any_tiles_occupied = nullptr) {
		std::array<static_vector<size_t, 240>, 8> target_tiles;
		bool spreads_creep = unit_type_spreads_creep(unit_type, true);
		auto area = get_max_creep_bb(unit_type, pos, true);
		int dy = (int)area.from.y * 32 - pos.y + 16;
		bool any_tiles_occupied = false;
		for (size_t tile_y = area.from.y; tile_y != area.to.y + 1; ++tile_y, dy += 32) {
			int dx = (int)area.from.x * 32 - pos.x + 16;
			for (size_t tile_x = area.from.x; tile_x != area.to.x + 1; ++tile_x, dx += 32) {
				size_t index = tile_y * game_st.map_tile_width + tile_x;
				auto flags = st.tiles[index].flags;
				if (flags & tile_t::flag_has_creep) continue;
				if (!tile_can_have_creep({tile_x, tile_y})) continue;
				if (flags & tile_t::flag_occupied) {
					if (!any_tiles_occupied) any_tiles_occupied = true;
					continue;
				}
				if (spreads_creep) {
					int d = dx*dx * 25 + dy*dy * 64;
					if (d > 320*320 * 25) continue;
				}
				size_t n = count_neighboring_creep_tiles({tile_x, tile_y});
				if (n == 0) continue;
				target_tiles[n - 1].push_back(index);
			}
		}
		if (out_any_tiles_occupied) *out_any_tiles_occupied = any_tiles_occupied;
		for (auto& v : reverse(target_tiles)) {
			if (v.empty()) continue;
			size_t index = v[(lcg_rand(26) >> 4) % v.size()];
			set_tile_creep({index % game_st.map_tile_width, index / game_st.map_tile_width});
			return true;
		}
		return false;
	}

	void spread_creep_completely(unit_type_autocast unit_type, xy pos) {
		rect_t<xy_t<size_t>> unit_area;
		unit_area.from.x = pos.x / 32u - unit_type->placement_size.x / 32u / 2;
		unit_area.from.y = pos.y / 32u - unit_type->placement_size.y / 32u / 2;
		unit_area.to.x = unit_area.from.x + unit_type->placement_size.x / 32u;
		unit_area.to.y = unit_area.from.y + unit_type->placement_size.y / 32u;
		for (size_t y = unit_area.from.y; y != unit_area.to.y; ++y) {
			for (size_t x = unit_area.from.x; x != unit_area.to.x; ++x) {
				if (!tile_can_have_creep({x, y})) continue;
				set_tile_creep({x, y});
			}
		}
		while (spread_creep(unit_type, pos));
	}

	xy get_spawn_larva_position(unit_t* u) {
		if (!unit_is_hatchery(u)) error("get_spawn_larva_position: unit is not a hatchery");
		int best_score = 101;
		xy best_pos(-1, -1);
		auto test = [&](size_t index, xy pos, xy neighbor_offset) {
			int val = u->building.hatchery.larva_spawn_side_values[index];
			if (val >= best_score) return false;
			if (restrict_move_target_to_valid_bounds(get_unit_type(UnitTypes::Zerg_Larva), pos) != pos) return false;
			if (~st.tiles[tile_index(pos)].flags & tile_t::flag_has_creep) return false;
			auto op = pos + neighbor_offset;
			if (restrict_move_target_to_valid_bounds(get_unit_type(UnitTypes::Zerg_Larva), op) != op) return false;
			auto flags = st.tiles[tile_index(op)].flags;
			if (flags & tile_t::flag_occupied) return false;
			if ((flags & (tile_t::flag_walkable | tile_t::flag_has_creep)) == 0) return false;
			best_score = val;
			best_pos = pos;
			return true;
		};
		auto bb = unit_sprite_inner_bounding_box(u);
		test(3, {u->sprite->position.x, bb.to.y + 10}, {0, 22});
		test(0, {bb.from.x - 10, u->sprite->position.y}, {-22, 0});
		test(2, {bb.to.x + 10, u->sprite->position.y}, {22, 0});
		test(1, {u->sprite->position.x, bb.from.y - 10}, {0, -22});
		if (best_pos != xy(-1, -1)) return best_pos;
		int w = (u->unit_type->dimensions.from.x + u->unit_type->dimensions.to.x + 1) / 2;
		int h = (u->unit_type->dimensions.from.y + u->unit_type->dimensions.to.y + 1) / 2;
		best_score = 0x10000;
		if (!test(3, {u->sprite->position.x - w, bb.to.y + 10}, {0, 22}) && !test(3, {u->sprite->position.x + w, bb.to.y + 10}, {0, 22})) {
			best_score = 101;
		}
		test(0, {bb.from.x - 10, u->sprite->position.y - h}, {-22, 0}) || test(0, {bb.from.x - 10, u->sprite->position.y + h}, {-22, 0});
		test(2, {bb.to.x + 10, u->sprite->position.y - h}, {22, 0}) || test(2, {bb.to.x + 10, u->sprite->position.y + h}, {22, 0});
		test(1, {u->sprite->position.x - w, bb.from.y - 10}, {0, -22}) || test(1, {u->sprite->position.x + w, bb.from.y - 10}, {0, -22});
		return best_pos;
	}

	unit_t* spawn_larva(unit_t* u) {
		xy pos = get_spawn_larva_position(u);
		if (pos == xy(-1, -1)) return nullptr;
		unit_t* larva = create_unit(get_unit_type(UnitTypes::Zerg_Larva), pos, u->owner);
		if (larva) {
			finish_building_unit(larva);
			complete_unit(larva);
			larva->connected_unit = u;
			if (larva->sprite->position.x < u->sprite->position.x - u->unit_type->dimensions.from.x) {
				larva->order_state = 0;
			} else if (larva->sprite->position.y < u->sprite->position.y - u->unit_type->dimensions.from.y) {
				larva->order_state = 1;
			} else if (larva->sprite->position.x > u->sprite->position.x + u->unit_type->dimensions.to.x) {
				larva->order_state = 2;
			} else {
				larva->order_state = 3;
			}
		}
		return larva;
	}



#endif // BWGAME_CREEP_H
