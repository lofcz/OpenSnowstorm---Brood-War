#ifndef BWGAME_TRIGGERS_H
#define BWGAME_TRIGGERS_H



	struct execute_trigger_struct {
		std::array<int, 12> victory_state{};
	};

	bool trigger_unit_matches_filter(const unit_t* u, int uid) const {
		if (uid == 229) return true;
		if (uid == 230) return (u->unit_type->group_flags & GroupFlags::Men) != 0;
		if (uid == 231) return (u->unit_type->group_flags & GroupFlags::Building) != 0;
		if (uid == 232) return (u->unit_type->group_flags & GroupFlags::Factory) != 0;
		return (int)u->unit_type->id == uid;
	}

	bool execute_trigger_action(execute_trigger_struct& ets, int owner, running_trigger& rt, running_trigger::action& ra, const trigger::action& a) {
		switch (a.type) {
		case 1: // victory
			if (st.players[owner].controller == player_t::controller_occupied || st.players[owner].controller == player_t::controller_computer_game) {
				ets.victory_state[owner] = 3;
			}
			return true;
		case 2: // defeat
			if (st.players[owner].controller == player_t::controller_occupied || st.players[owner].controller == player_t::controller_computer_game) {
				ets.victory_state[owner] = 2;
			}
			return true;
		case 3: // preserve trigger
			rt.flags |= 4;
			return true;
		case 4: // wait
			if (st.trigger_waiting[owner]) return false;
			if (ra.flags & 1) {
				ra.flags &= ~1;
				return true;
			}
			if (rt.flags & 0x10) return true;
			st.trigger_waiting[owner] = true;
			st.trigger_wait_timers[owner] = a.time_n;
			ra.flags |= 1;
			return false;
		case 5: // pause game – notify UI layer
			on_trigger_pause_game();
			return true;
		case 6: // unpause game – notify UI layer
			on_trigger_unpause_game();
			return true;
		case 7: // transmission (voice + text + center view)
			on_trigger_transmission(owner, a.string_index, a.sound_index, a.extra_n, a.time_n, a.location);
			return true;
		case 8: // play WAV
			if (a.string_index > 0) {
				// No location or source_unit for global trigger sounds.
				play_sound(a.string_index);
			}
			return true;
		case 9: // display text message
			if (a.string_index > 0) {
				a_string msg = get_map_string((size_t)a.string_index);
				on_trigger_display_text(owner, msg);
			}
			return true;
		case 10: // center view - handled at UI layer; simulation logs the location
			on_trigger_center_view(owner, a.location);
			return true;
		case 11: // create unit with properties - treat same as action 44
			for (int p : trigger_players(owner, a.group_n)) {
				if (a.extra_n < 0 || a.extra_n >= 228) continue;
				const unit_type_t* ut = get_unit_type((UnitTypes)a.extra_n);
				if (unit_is_mineral_field(ut) || unit_is(ut, UnitTypes::Resource_Vespene_Geyser)) p = 11;
				auto& loc = st.locations.at((size_t)a.location - 1);
				xy pos = (loc.area.from + loc.area.to) / 2;
				for (int i = 0; i != a.num_n; ++i) {
					trigger_create_unit(ut, pos, p);
				}
			}
			return true;
		case 12: // set mission objectives
			if (a.string_index > 0) {
				a_string obj = get_map_string((size_t)a.string_index);
				on_trigger_set_objectives(owner, obj);
			}
			return true;
		case 13: // move unit to location
			{
				const location& src_loc = st.locations.at((size_t)a.location - 1);
				const location& dst_loc = st.locations.at((size_t)a.group2_n - 1);
				xy dst_pos = (dst_loc.area.from + dst_loc.area.to) / 2;
				int moved = 0;
				int limit = a.num_n == 0 ? std::numeric_limits<int>::max() : a.num_n;
				a_vector<unit_t*> to_move;
				for (unit_t* u : find_units(src_loc.area)) {
					if (moved >= limit) break;
					if (!unit_is_at_elevation_flags(u, src_loc.elevation_flags)) continue;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
					if (!trigger_unit_matches_filter(u, a.extra_n)) continue;
					if (ut_turret(u)) continue;
					if (unit_dying(u)) continue;
					to_move.push_back(u);
					++moved;
				}
				for (unit_t* u : to_move) {
					move_unit(u, dst_pos);
				}
			}
			return true;
		case 15: // ai script
			switch (a.group2_n) {
			case 0x3069562b:
				st.shared_vision[0] |= 1 << owner;
				break;
			case 0x3169562b:
				st.shared_vision[1] |= 1 << owner;
				break;
			case 0x3269562b:
				st.shared_vision[2] |= 1 << owner;
				break;
			case 0x3369562b:
				st.shared_vision[3] |= 1 << owner;
				break;
			case 0x3469562b:
				st.shared_vision[4] |= 1 << owner;
				break;
			case 0x3569562b:
				st.shared_vision[5] |= 1 << owner;
				break;
			case 0x3669562b:
				st.shared_vision[6] |= 1 << owner;
				break;
			case 0x3769562b:
				st.shared_vision[7] |= 1 << owner;
				break;
			default:
				// Unknown AI script tags are silently ignored.
				break;
			}
			return true;
		case 16: // set alliance status
			for (int p : trigger_players(owner, a.group_n)) {
				if (p < 0 || p >= 12) continue;
				for (int q : trigger_players(owner, a.group2_n)) {
					if (q < 0 || q >= 12 || q == p) continue;
					if (a.extra_n == 0) st.alliances[p][q] = 0;
					else st.alliances[p][q] = 2;
				}
			}
			return true;
		case 17: // set score
			for (int p : trigger_players(owner, a.group_n)) {
				if (p < 0 || p >= 12) continue;
				int val = a.group2_n;
				int* score = nullptr;
				if (a.extra_n == 0 || a.extra_n == 1 || a.extra_n == 3) score = &st.unit_score[p];
				else if (a.extra_n == 2)                                  score = &st.building_score[p];
				if (!score) continue;
				if (a.num_n == 7) *score = val;
				else if (a.num_n == 8) *score += val;
				else if (a.num_n == 9) { *score -= val; if (*score < 0) *score = 0; }
			}
			return true;
		case 21: // set countdown timer
			if (a.num_n == 7) st.countdown_timer = a.time_n;
			else if (a.num_n == 8) st.countdown_timer += a.time_n;
			else if (a.num_n == 9) { st.countdown_timer -= a.time_n; if (st.countdown_timer < 0) st.countdown_timer = 0; }
			return true;
		case 22: // kill unit
			for (int p : trigger_players(owner, a.group_n)) {
				int uid = a.extra_n;
				for (auto i = st.player_units[p].begin(); i != st.player_units[p].end();) {
					unit_t* u = &*i++;
					if (ut_turret(u)) continue;
					if (unit_dying(u)) continue;
					if (ut_powerup(u)) continue;
					if (u->owner != p) continue;
					if (trigger_unit_matches_filter(u, uid)) kill_unit(u);
				}
			}
			return true;
		case 23: // kill unit at location
			for (int p : trigger_players(owner, a.group_n)) {
				int uid = a.extra_n;
				const location& loc = st.locations.at((size_t)a.location - 1);
				a_vector<unit_t*> to_kill;
				for (unit_t* u : find_units(loc.area)) {
					if (!unit_is_at_elevation_flags(u, loc.elevation_flags)) continue;
					if (ut_turret(u)) continue;
					if (unit_dying(u)) continue;
					if (ut_powerup(u)) continue;
					if (u->owner != p) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					to_kill.push_back(u);
				}
				for (unit_t* u : to_kill) kill_unit(u);
			}
			return true;
		case 24: // remove unit
			for (int p : trigger_players(owner, a.group_n)) {
				int uid = a.extra_n;
				for (auto i = st.player_units[p].begin(); i != st.player_units[p].end();) {
					unit_t* u = &*i++;
					if (ut_turret(u)) continue;
					if (unit_dying(u)) continue;
					if (ut_powerup(u)) continue;
					if (u->owner != p) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					hide_unit(u);
					kill_unit(u);
				}
			}
			return true;
		case 25: // remove unit at location
			{
				int uid = a.extra_n;
				std::function<void(unit_t*)> proc = [&](unit_t* u) {
					if (!unit_is_at_elevation_flags(u, st.locations[a.location - 1].elevation_flags)) return;

					if (unit_provides_space(u)) {
						for (unit_t* n : loaded_units(u)) {
							proc(n);
						}
					}
					if (ut_worker(u) && u->worker.powerup) {
						proc(u->worker.powerup);
					}

					if (ut_turret(u)) return;
					if (unit_dying(u)) return;
					if (ut_powerup(u)) return;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) return;
					if (!trigger_unit_matches_filter(u, uid)) return;
					hide_unit(u);
					kill_unit(u);
				};
				for (unit_t* u : find_units(st.locations.at(a.location - 1).area)) {
					proc(u);
				}
			}
			return true;
		case 26: // set resources
			for (int p : trigger_players(owner, a.group_n)) {
				if (p >= 8) continue;
				if (a.num_n == 7) {         // set
					if (a.extra_n == 0 || a.extra_n == 2) {
						st.current_minerals[p] = a.group2_n;
						st.total_minerals_gathered[p] = a.group2_n;
					}
					if (a.extra_n == 1 || a.extra_n == 2) {
						st.current_gas[p] = a.group2_n;
						st.total_gas_gathered[p] = a.group2_n;
					}
				} else if (a.num_n == 8) {  // add
					if (a.extra_n == 0 || a.extra_n == 2) {
						st.current_minerals[p] += a.group2_n;
						st.total_minerals_gathered[p] += a.group2_n;
					}
					if (a.extra_n == 1 || a.extra_n == 2) {
						st.current_gas[p] += a.group2_n;
						st.total_gas_gathered[p] += a.group2_n;
					}
				} else if (a.num_n == 9) {  // subtract
					if (a.extra_n == 0 || a.extra_n == 2) {
						if (st.current_minerals[p] < a.group2_n) st.current_minerals[p] = 0;
						else st.current_minerals[p] -= a.group2_n;
					}
					if (a.extra_n == 1 || a.extra_n == 2) {
						if (st.current_gas[p] < a.group2_n) st.current_gas[p] = 0;
						else st.current_gas[p] -= a.group2_n;
					}
				}
			}
			return true;
		case 27: case 28: case 29: case 30: case 31: // leaderboard actions
			return true;
		case 32: // draw
			for (int p : active_players()) {
				ets.victory_state[p] = 5;
			}
			return true;
		case 33: // set alliance status (alternate form)
			for (int p : trigger_players(owner, a.group_n)) {
				if (p < 0 || p >= 12) continue;
				for (int q : trigger_players(owner, a.group2_n)) {
					if (q < 0 || q >= 12 || q == p) continue;
					if (a.extra_n == 0) st.alliances[p][q] = 0;
					else st.alliances[p][q] = 2;
				}
			}
			return true;
		case 36: // give units to player
			{
				int dst = a.group2_n;
				if (dst < 0 || dst >= 12) return true;
				int uid = a.extra_n;
				int limit = a.num_n == 0 ? std::numeric_limits<int>::max() : a.num_n;
				const location& loc = st.locations.at((size_t)a.location - 1);
				a_vector<unit_t*> to_give;
				for (unit_t* u : find_units(loc.area)) {
					if ((int)to_give.size() >= limit) break;
					if (!unit_is_at_elevation_flags(u, loc.elevation_flags)) continue;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					if (ut_turret(u)) continue;
					if (unit_dying(u)) continue;
					to_give.push_back(u);
				}
				for (unit_t* u : to_give) {
					trigger_give_unit_to(u, dst);
				}
			}
			return true;
		case 38: // move location
			{
				auto& loc = st.locations.at(a.location - 1);
				xy pos = (loc.area.from + loc.area.to) / 2;
				unit_t* u = trigger_find_unit(owner, loc, a.group_n, a.extra_n);
				if (u) pos = u->sprite->position;
				auto& target_loc = st.locations.at(a.group2_n - 1);
				xy diff = pos - (loc.area.from + loc.area.to) / 2;
				xy from = target_loc.area.from + diff;
				xy to = target_loc.area.to + diff;
				if (from.x < 0) { to.x -= from.x; from.x = 0; }
				if (from.y < 0) { to.y -= from.y; from.y = 0; }
				if (to.x >= (int)game_st.map_width) { from.x += (int)game_st.map_width - 1 - to.x; to.x = (int)game_st.map_width - 1; }
				if (to.y >= (int)game_st.map_height) { from.y += (int)game_st.map_height - 1 - to.y; to.y = (int)game_st.map_height - 1; }
				target_loc.area.from = from;
				target_loc.area.to = to;
			}
			return true;
		case 40: // set switch
			if (a.extra_n >= 0 && a.extra_n < 256) {
				if (a.num_n == 4)       st.switches[(size_t)a.extra_n] = true;
				else if (a.num_n == 5)  st.switches[(size_t)a.extra_n] = false;
				else if (a.num_n == 6)  st.switches[(size_t)a.extra_n] = !st.switches[(size_t)a.extra_n];
				else if (a.num_n == 11) st.switches[(size_t)a.extra_n] = (lcg_rand(78) & 1) != 0;
			}
			return true;
		case 44: // create unit
			for (int p : trigger_players(owner, a.group_n)) {
				if (a.extra_n < 0 || a.extra_n >= 228) continue;
				const unit_type_t* ut = get_unit_type((UnitTypes)a.extra_n);
				if (unit_is_mineral_field(ut) || unit_is(ut, UnitTypes::Resource_Vespene_Geyser)) p = 11;
				auto& loc = st.locations.at(a.location - 1);
				xy pos = (loc.area.from + loc.area.to) / 2;
				for (int i = 0; i != a.num_n; ++i) {
					trigger_create_unit(ut, pos, p);
				}
			}
			return true;
		case 45: // set deaths
			for (int p : trigger_players(owner, a.group_n)) {
				if (p < 0 || p >= 12) continue;
				int uid = a.extra_n;
				if (uid < 0 || uid >= 228) continue;
				int val = a.group2_n;
				if (a.num_n == 7)      st.unit_deaths[(size_t)p][(size_t)uid] = val;
				else if (a.num_n == 8) st.unit_deaths[(size_t)p][(size_t)uid] += val;
				else if (a.num_n == 9) {
					st.unit_deaths[(size_t)p][(size_t)uid] -= val;
					if (st.unit_deaths[(size_t)p][(size_t)uid] < 0)
						st.unit_deaths[(size_t)p][(size_t)uid] = 0;
				}
			}
			return true;
		case 46: // order
			for (unit_t* u : find_units(st.locations.at(a.location - 1).area)) {
				if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
				if (!unit_is_at_elevation_flags(u, st.locations[a.location - 1].elevation_flags)) continue;
				if (ut_building(u)) {
					if (u->order_type->id == Orders::BuildingLand || u->order_type->id == Orders::BuildingLiftoff) continue;
				}
				if (!u_can_move(u)) continue;
				if (unit_is(u, UnitTypes::Terran_Nuclear_Missile)) continue;
				if (unit_is_fighter(u)) continue;
				if (unit_is_rescuable(u)) continue;
				if (!trigger_unit_matches_filter(u, a.extra_n)) continue;
				Orders o = Orders::Nothing;
				if (a.num_n == 0) o = Orders::Move;
				else if (a.num_n == 1) o = Orders::Patrol;
				else if (a.num_n == 2) o = Orders::AttackMove;
				const order_type_t* order = get_order_type(o);
				auto& target_loc = st.locations.at(a.group2_n - 1);
				xy target_pos = (target_loc.area.from + target_loc.area.to) / 2;
				if (u_burrowed(u)) {
					unburrow_unit(u);
					set_queued_order(u, false, order, target_pos);
				} else {
					if (!unit_can_receive_order(u, order, u->owner)) continue;
					set_unit_order(u, order, target_pos);
				}
			}
			return true;
		case 47: // play WAV (alternate form)
			return true;
		case 48: // modify unit hit points
			{
				int uid = a.extra_n;
				int pct = a.group2_n;
				const location& loc = st.locations.at((size_t)a.location - 1);
				for (unit_t* u : find_units(loc.area)) {
					if (!unit_is_at_elevation_flags(u, loc.elevation_flags)) continue;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					if (unit_dying(u)) continue;
					fp8 new_hp = u->unit_type->hitpoints * pct / 100;
					if (new_hp < 1_fp8) new_hp = 1_fp8;
					set_unit_hp(u, new_hp);
				}
			}
			return true;
		case 49: // modify unit energy
			{
				int uid = a.extra_n;
				int pct = a.group2_n;
				const location& loc = st.locations.at((size_t)a.location - 1);
				for (unit_t* u : find_units(loc.area)) {
					if (!unit_is_at_elevation_flags(u, loc.elevation_flags)) continue;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					if (unit_dying(u)) continue;
					fp8 max_e = unit_max_energy(u);
					set_unit_energy(u, max_e * pct / 100);
				}
			}
			return true;
		case 50: // modify unit shield points
			{
				int uid = a.extra_n;
				int pct = a.group2_n;
				const location& loc = st.locations.at((size_t)a.location - 1);
				for (unit_t* u : find_units(loc.area)) {
					if (!unit_is_at_elevation_flags(u, loc.elevation_flags)) continue;
					if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
					if (!trigger_unit_matches_filter(u, uid)) continue;
					if (unit_dying(u)) continue;
					if (!u->unit_type->has_shield) continue;
					fp8 new_sp = fp8::integer(u->unit_type->shield_points) * pct / 100;
					set_unit_shield_points(u, new_sp);
				}
			}
			return true;
		case 51: // modify unit resource amount
			for (unit_t* u : find_units(st.locations.at(a.location - 1).area)) {
				if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
				if (!unit_is_at_elevation_flags(u, st.locations[a.location - 1].elevation_flags)) continue;
				if (!trigger_unit_matches_filter(u, a.extra_n)) continue;
				set_unit_resources(u, a.group2_n);
			}
			return true;
		case 52: // set unit resources
			for (unit_t* u : find_units(st.locations.at(a.location - 1).area)) {
				if (!trigger_players_pred(owner, a.group_n, u->owner)) continue;
				if (!unit_is_at_elevation_flags(u, st.locations[a.location - 1].elevation_flags)) continue;
				set_unit_resources(u, a.group2_n);
			}
			return true;
		case 53: // modify unit hangar count
			return true;
		case 55: // minimap ping
			{
				auto& loc = st.locations.at(a.location - 1);
				xy pos = (loc.area.from + loc.area.to) / 2;
				on_trigger_minimap_ping(owner, pos);
			}
			return true;
		case 56: // talking portrait
			on_trigger_talking_portrait(owner, a.extra_n, a.time_n, a.group_n);
			return true;
		case 57: // mute unit speech
			return true;
		case 58: // set next scenario
			if (a.string_index > 0) {
				a_string scenario = get_map_string((size_t)a.string_index);
				on_trigger_set_next_scenario(owner, scenario);
			}
			return true;
		case 59: // set countdown timer (alternate form)
			if (a.num_n == 7) st.countdown_timer = a.time_n;
			else if (a.num_n == 8) st.countdown_timer += a.time_n;
			else if (a.num_n == 9) { st.countdown_timer -= a.time_n; if (st.countdown_timer < 0) st.countdown_timer = 0; }
			return true;
		case 61: case 62: case 63: case 64: // leaderboard goal actions
			return true;
		default:
			return true;
		}
	}
	void execute_trigger(execute_trigger_struct& ets, int owner, running_trigger& rt, const trigger& t) {
		rt.flags |= 1;
		size_t index = rt.current_action_index;
		for (;index != 64; ++index) {
			auto& a = t.actions[index];
			if (a.flags & 2) continue;
			if (a.type == 0) index = 63;
			else if (!execute_trigger_action(ets, owner, rt, rt.actions[index], a)) break;
		}
		rt.current_action_index = index;
		if (index == 64) {
			if (rt.flags & 4) {
				rt.current_action_index = 0;
				rt.flags &= ~0x51;
			} else rt.flags |= 8;
		}
	}



#endif // BWGAME_TRIGGERS_H
