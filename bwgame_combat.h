	void on_unit_damage(unit_t* u, unit_t* source_unit, bool reveal_source) {
		if (!source_unit || source_unit->owner == u->owner) return;
		u->last_attacking_player = source_unit->owner;
		if (reveal_source && !unit_target_is_undetected(u, source_unit) && u->owner < 8) {
			reveal_sight_at(source_unit->sprite->position, 1, 1 << u->owner, u_flying(source_unit));
		}
		if (u_in_bunker(source_unit)) {
			source_unit = source_unit->connected_unit;
		}
		if (on_hit_should_change_target(u, u->auto_target_unit, source_unit)) {
			on_hit_change_target(u, source_unit);
		}
		rect bb{u->sprite->position - xy(96, 96), u->sprite->position + xy(96, 96)};
		for (unit_t* n : find_units_noexpand(bb)) {
			if (n != u && n->owner == u->owner) {
				on_hit_aoe_change_target(n, source_unit);
			}
		}
	}

	void unit_deal_damage(unit_t* u, fp8 damage, unit_t* source_unit, int source_owner, bool reveal_source = true) {
		(void)source_owner;
		if (u->hp == 0_fp8) return;
		if (st.cheat_power_overwhelming && u->owner < 8) return;
		on_unit_damage(u, source_unit, reveal_source);
		if (damage < u->hp) {
			u->hp -= damage;
			u->air_strength = get_unit_strength(u, false);
			u->ground_strength = get_unit_strength(u, true);
			if (u_completed(u)) {
				update_unit_damage_overlay(u);
			}
			if (source_unit && source_unit->owner != u->owner && reveal_source) {
				// todo: callback for notifications?
			}
		} else {
			if (unit_provides_space(u) && !ut_building(u)) {
				for (unit_t* n : loaded_units(u)) {
					kill_unit(n);
					if (n->owner < 12) {
						if (ut_building(n)) st.building_score[n->owner] -= n->unit_type->build_score;
						else st.unit_score[n->owner] -= n->unit_type->build_score;
					}
				}
			}
			u->hp = 0_fp8;
			kill_unit(u);
			if (u->owner < 12) {
				if (ut_building(u)) st.building_score[u->owner] -= u->unit_type->build_score;
				else st.unit_score[u->owner] -= u->unit_type->build_score;
			}
			if (source_unit && unit_target_is_enemy(source_unit, u)) {
				if (source_unit->owner < 12) {
					st.unit_deaths[source_unit->owner][(size_t)u->unit_type->id]++;
				}
			}
		}
	}

	void create_shield_damage_effect(unit_t* target, direction_t heading) {
		size_t index = (direction_index(heading) - 124) / 8 % 32;
		xy offset = get_image_lo_offset(target->sprite->main_image, 5, index, true);
		auto* image = create_image(get_image_type(ImageTypes::IMAGEID_Shield_Overlay), target->sprite, offset, image_order_above);
		if (image) set_image_heading_by_index(image, index);
	}

	void weapon_deal_damage(const weapon_type_t* weapon, fp8 damage, int damage_divisor, unit_t* target, direction_t heading, unit_t* source_unit, int source_owner) {
		if (target->hp == 0_fp8) return;
		if (u_invincible(target)) return;
		if (u_hallucination(target)) damage *= 2;
		damage /= damage_divisor;
		damage += fp8::integer(target->acid_spore_count);
		if (damage < 128_fp8) damage = 128_fp8;
		if (target->defensive_matrix_hp != 0_fp8) {
			fp8 defensive_matrix_damage = damage;
			if (damage > target->defensive_matrix_hp) defensive_matrix_damage = target->defensive_matrix_hp;
			deal_defensive_matrix_damage(target, defensive_matrix_damage);
			damage -= defensive_matrix_damage;
		}
		fp8 shield_damage = 0_fp8;
		if (target->unit_type->has_shield) {
			if (target->shield_points >= 256_fp8) {
				if (weapon->damage_type != weapon_type_t::damage_type_ignore_armor) {
					fp8 shield_armor = fp8::integer(player_upgrade_level(target->owner, UpgradeTypes::Protoss_Plasma_Shields));
					if (damage > shield_armor) damage -= shield_armor;
					else damage = 128_fp8;
				}
				shield_damage = damage;
				if (shield_damage > target->shield_points) shield_damage = target->shield_points;
				damage -= shield_damage;
			}
		}
		if (weapon->damage_type != weapon_type_t::damage_type_ignore_armor) {
			fp8 armor = unit_armor(target);
			if (damage > armor) damage -= armor;
			else damage = 0_fp8;
		}
		switch (weapon->damage_type) {
		case weapon_type_t::damage_type_none:
			damage = 0_fp8;
			break;
		case weapon_type_t::damage_type_explosive:
			if (target->unit_type->unit_size == 0) damage = 0_fp8;
			else if (target->unit_type->unit_size == 1) damage *= 128_fp8;
			else if (target->unit_type->unit_size == 2) damage *= 192_fp8;
			break;
		case weapon_type_t::damage_type_concussive:
			if (target->unit_type->unit_size == 0) damage = 0_fp8;
			else if (target->unit_type->unit_size == 2) damage *= 128_fp8;
			else if (target->unit_type->unit_size == 3) damage *= 64_fp8;
			break;
		case weapon_type_t::damage_type_normal:
			break;
		case weapon_type_t::damage_type_ignore_armor:
			break;
		}
		if (shield_damage == 0_fp8 && damage < 128_fp8) damage = 128_fp8;
		unit_deal_damage(target, damage, source_unit, source_owner, weapon->id != WeaponTypes::Irradiate);
		if (shield_damage != 0_fp8) {
			target->shield_points -= shield_damage;
			if (weapon->damage_type != weapon_type_t::damage_type_none && target->shield_points != 0_fp8) {
				create_shield_damage_effect(target, heading);
			}
		}
		target->air_strength = get_unit_strength(target, false);
		target->ground_strength = get_unit_strength(target, true);
	}

	void hallucinated_weapon_hit(const weapon_type_t* weapon, unit_t* target, direction_t heading, unit_t* source_unit) {
		if (target->hp == 0_fp8 || u_invincible(target)) return;
		if (source_unit) {
			on_unit_damage(target, source_unit, weapon->id != WeaponTypes::Irradiate);
			if (weapon->id != WeaponTypes::Irradiate && target->owner != source_unit->owner) {
				// todo: callback for notifications?
			}
			if (weapon->damage_type != weapon_type_t::damage_type_none) {
				if (target->unit_type->has_shield && target->shield_points >= fp8::integer(1)) {
					create_shield_damage_effect(target, heading);
				}
			}
		}
	}

	void bullet_deal_damage(const bullet_t* b, unit_t* target, int damage_divisor) {
		if (b->hit_flags & 2) {
			hallucinated_weapon_hit(b->weapon_type, target, b->heading, b->bullet_owner_unit);
		} else {
			weapon_deal_damage(b->weapon_type, bullet_damage_amount(b, target), damage_divisor, target, b->heading, b->bullet_owner_unit, b->owner);
		}
	}
	void bullet_deal_damage(const bullet_t* b, unit_t* target) {
		bullet_deal_damage(b, target, 1);
	}

	void melee_deal_damage(unit_t* u) {
		unit_t* target = u->order_target.unit;
		if (!target) return;
		auto* w = unit_ground_weapon(u);
		if (u_hallucination(u)) {
			hallucinated_weapon_hit(w, target, u->heading, u);
		} else {
			weapon_deal_damage(w, weapon_damage_amount(w, u->owner), 1, target, u->heading, u, u->owner);
		}
	}

	bool weapon_can_target_unit(const weapon_type_t* weapon, const unit_t* target) const {
		if (!target) return (weapon->target_flags & 0x40) != 0;
		if (u_invincible(target)) return false;
		if (~weapon->target_flags & (u_flying(target) ? 1 : 2)) return false;
		if (weapon->target_flags & 4 && !ut_mechanical(target)) return false;
		if (weapon->target_flags & 8 && !ut_organic(target)) return false;
		if (weapon->target_flags & 0x10 && ut_building(target)) return false;
		if (weapon->target_flags & 0x20 && ut_robotic(target)) return false;
		if (weapon->target_flags & 0x80 && !ut_mechanical(target) && !ut_organic(target)) return false;
		return true;
	}
	bool weapon_can_target_unit(const weapon_type_t* weapon, const unit_t* target, const unit_t* source_unit) const {
		if (!weapon_can_target_unit(weapon, target)) return false;
		if (target && weapon->target_flags & 0x100 && target->owner != source_unit->owner) return false;
		return true;
	}

	template<bool is_air_splash>
	void bullet_deal_splash_damage(bullet_t* b) {
		auto bb = square_at(b->sprite->position, b->weapon_type->outer_splash_radius);
		if (is_air_splash) {
			a_vector<unit_t*> targets;
			for (unit_t* target : find_units(bb)) {
				if (target == b->bullet_owner_unit) continue;
				if (target->owner == b->owner && target != b->bullet_target) continue;
				if (!weapon_can_target_unit(b->weapon_type, target)) continue;
				int distance = unit_distance_to(target, b->sprite->position);
				if (distance > b->weapon_type->outer_splash_radius) continue;
				if (target == b->bullet_target && distance <= b->weapon_type->inner_splash_radius) {
					targets = {target};
					break;
				}
				targets.push_back(target);
			}
			if (!targets.empty()) {
				size_t random_index = 0;
				if (targets.size() != 1) random_index = lcg_rand(58) % targets.size();
				bullet_deal_damage(b, targets[random_index]);
			}
		}
		bool damages_allies = !is_air_splash && b->weapon_type->hit_type != weapon_type_t::hit_type_enemy_splash;
		for (unit_t* target : find_units(bb)) {
			if (target == b->bullet_owner_unit && b->weapon_type->id != WeaponTypes::Psionic_Storm) continue;
			if (!damages_allies && target->owner == b->owner && target != b->bullet_target) continue;
			if (!weapon_can_target_unit(b->weapon_type, target)) continue;
			int distance = unit_distance_to(target, b->sprite->position);
			if (distance > b->weapon_type->outer_splash_radius) continue;
			if (b->weapon_type->id == WeaponTypes::Psionic_Storm) {
				if (target->storm_timer) continue;
				target->storm_timer = 1;
			}
			if (is_air_splash) {
				if (target == b->bullet_target) continue;
				if (unit_is(target, UnitTypes::Protoss_Interceptor)) continue;
			}
			if (!is_air_splash && distance <= b->weapon_type->inner_splash_radius) {
				bullet_deal_damage(b, target, 1);
			} else if (!u_burrowed(target)) {
				if (distance <= b->weapon_type->medium_splash_radius) {
					bullet_deal_damage(b, target, 2);
				} else {
					bullet_deal_damage(b, target, 4);
				}
			}
		}
	}

	void irradiate_unit(unit_t* u, unit_t* source_unit, int source_owner) {
		if (u->irradiate_timer == 0 && !u_burrowed(u)) {
			create_sized_image(u, ImageTypes::IMAGEID_Irradiate_Small);
		}
		u->irradiate_timer = get_weapon_type(WeaponTypes::Irradiate)->cooldown;
		u->irradiated_by = source_unit;
		u->irradiate_owner = source_owner;
	}

	void return_interceptors(unit_t* u) {
		if (!unit_is_carrier(u)) return;
		for (unit_t* n : ptr(u->carrier.outside_units)) {
			set_unit_order(n, get_order_type(Orders::InterceptorReturn));
		}
	}

	void set_unit_disabled(unit_t* u) {
		if (u->subunit) u_set_status_flag(u->subunit, unit_t::status_flag_disabled);
		stop_unit(u);
		if (unit_is_ghost(u) && u->connected_unit && unit_is(u->connected_unit, UnitTypes::Terran_Nuclear_Missile)) {
			u->connected_unit->connected_unit = nullptr;
			u->connected_unit = nullptr;
		}
		if (unit_is_carrier(u)) return_interceptors(u);
		if (u->order_type->id == Orders::Repair || u->order_type->id == Orders::DroneLand) u->order_state = 0;
		if (!u->order_type->unk9) u->order_target.unit = nullptr;
		iscript_run_to_idle(u);
	}

	void lockdown_unit(unit_t* u) {
		if (u->lockdown_timer == 0) {
			create_sized_image(u, ImageTypes::IMAGEID_Lockdown_Field_Small);
		}
		if (u->lockdown_timer < 131) u->lockdown_timer = 131;
		set_unit_disabled(u);
	}

	void blind_unit(unit_t* u, int source_owner) {
		if (u_hallucination(u)) {
			kill_unit(u);
			return;
		}
		u->blinded_by |= 1 << source_owner;
		play_sound(1019, u);
		create_sized_image(u, ImageTypes::IMAGEID_Optical_Flare_Hit_Small);
		st.update_tiles_countdown = 1;
	}

	void restore_unit(unit_t* u) {
		if (u_hallucination(u)) {
			kill_unit(u);
			return;
		}
		create_sized_image(u, ImageTypes::IMAGEID_Restoration_Hit_Small);
		u->parasite_flags = 0;
		u->blinded_by = 0;
		if (u->ensnare_timer) remove_ensnare(u);
		if (u->plague_timer) remove_plague(u);
		if (u->irradiate_timer) remove_irradiate(u);
		if (u->lockdown_timer) remove_lockdown(u);
		if (u->maelstrom_timer) remove_maelstrom(u);
		if (u->acid_spore_count) remove_acid_spores(u);
	}

	void emp_shockwave(xy position, const unit_t* source_unit) {
		int range = get_weapon_type(WeaponTypes::EMP_Shockwave)->inner_splash_radius;
		for (unit_t* target : find_units_noexpand(square_at(position, range))) {
			if (target == source_unit) continue;
			if (source_unit && target == source_unit->subunit) continue;
			if (u_hallucination(target)) {
				kill_unit(target);
				continue;
			}
			if (target->stasis_timer) continue;
			target->energy = 0_fp8;
			target->shield_points = 0_fp8;
		}
	}

	void spawn_broodlings(unit_t* target, const unit_t* source_unit) {
		if (!u_hallucination(target)) {
			const unit_type_t* broodling_type = get_unit_type(UnitTypes::Zerg_Broodling);
			auto spawn = [&](xy pos) {
				if (!unit_type_can_fit_at(broodling_type, target->sprite->position)) {
					if (!find_unit_placement(source_unit, pos, {pos - xy(32, 32), pos + xy(32, 32)}, false).first) return;
				}
				pos = restrict_move_target_to_valid_bounds(broodling_type, pos);
				unit_t* broodling = create_unit(broodling_type, pos, source_unit->owner);
				if (broodling) {
					finish_building_unit(broodling);
					complete_unit(broodling);
					set_remove_timer(broodling);
				} else display_last_error_for_player(source_unit->owner);
			};
			spawn(target->sprite->position - xy(2, 2));
			spawn(target->sprite->position + xy(2, 2));
		}
		// todo: increment scores and kill count
		kill_unit(target);
	}

	void ensnare_unit(unit_t* u) {
		if (!u->ensnare_timer && !u_burrowed(u)) {
			create_sized_image(u, ImageTypes::IMAGEID_Ensnare_Overlay_Small);
		}
		u->ensnare_timer = 75;
		update_unit_speed(u);
	}

	void ensnare(xy pos, const unit_t* source_unit) {
		thingy_t* t = create_thingy(get_sprite_type(SpriteTypes::SPRITEID_Ensnare), pos, 0);
		t->sprite->elevation_level = 19;
		if (!us_hidden(t)) set_sprite_visibility(t->sprite, tile_visibility(t->sprite->position));
		for (unit_t* target : find_units_noexpand(square_at(pos, 64))) {
			if (target == source_unit) continue;
			if (ut_building(target)) continue;
			if (u_invincible(target)) continue;
			if (u_burrowed(target)) continue;
			ensnare_unit(target);
		}
	}

	void consume_unit(unit_t* target, unit_t* source_unit) {
		if (!target || u_invincible(target)) return;
		if (!unit_tech_target_valid(source_unit, get_tech_type(TechTypes::Consume), target)) return;
		// todo: scores
		kill_unit(target);
		if (!u_hallucination(target)) {
			source_unit->energy = std::min(source_unit->energy + fp8::integer(50), unit_max_energy(source_unit));
		}
	}

	void dark_swarm(xy pos, int owner) {
		const unit_type_t* unit_type = get_unit_type(UnitTypes::Spell_Dark_Swarm);
		pos = restrict_move_target_to_valid_bounds(unit_type, pos);
		unit_t* u = create_unit(unit_type, pos, 11);
		if (u) {
			u_set_status_flag(u, unit_t::status_flag_no_collide);
			u->sprite->elevation_level = 11;
			finish_building_unit(u);
			complete_unit(u);
			set_remove_timer(u, 900);
		} else display_last_error_for_player(owner);
	}

	void plague_unit(unit_t* u) {
		if (!u->plague_timer && !u_burrowed(u)) {
			create_sized_image(u, ImageTypes::IMAGEID_Plague_Overlay_Small);
		}
		u->plague_timer = 75;
	}

	void plague(xy pos, unit_t* source_unit) {
		for (unit_t* target : find_units_noexpand(square_at(pos, 64))) {
			if (target == source_unit) continue;
			if (u_invincible(target)) continue;
			if (u_burrowed(target)) continue;
			plague_unit(target);
			if (source_unit) on_unit_damage(target, source_unit, true);
		}
	}

	void parasite_unit(unit_t* u, int owner) {
		u->parasite_flags |= 1 << owner;
	}

	void stasis_unit(unit_t* u) {
		if (u->stasis_timer == 0) {
			create_sized_image(u, ImageTypes::IMAGEID_Stasis_Field_Small);
			u_set_status_flag(u, unit_t::status_flag_invincible);
		}
		if (u->stasis_timer < 131) u->stasis_timer = 131;
		set_unit_disabled(u);
	}

	void stasis_field(xy pos, unit_t* source_unit) {
		thingy_t* t = create_thingy(get_sprite_type(SpriteTypes::SPRITEID_Stasis_Field_Hit), pos, 0);
		if (t) {
			t->sprite->elevation_level = 17;
			if (!us_hidden(t)) set_sprite_visibility(t->sprite, tile_visibility(t->sprite->position));
		}
		for (unit_t* target : find_units_noexpand(square_at(pos, 48))) {
			if (target == source_unit) continue;
			if (u_invincible(target)) continue;
			if (u_burrowed(target)) continue;
			if (ut_building(target)) continue;
			stasis_unit(target);
		}
	}

	void maelstrom_unit(unit_t* u) {
		if (u->maelstrom_timer == 0) {
			create_sized_image(u, ImageTypes::IMAGEID_Maelstorm_Overlay_Small);
		}
		if (u->maelstrom_timer < 22) u->maelstrom_timer = 22;
		set_unit_disabled(u);
	}

	void maelstrom(xy pos, unit_t* source_unit) {
		thingy_t* t = create_thingy(get_sprite_type(SpriteTypes::SPRITEID_Maelstrom_Hit), pos, 0);
		if (t) {
			t->sprite->elevation_level = 17;
			if (!us_hidden(t)) set_sprite_visibility(t->sprite, tile_visibility(t->sprite->position));
		}
		play_sound(1064, source_unit);
		for (unit_t* target : find_units_noexpand(square_at(pos, 48))) {
			if (u_hallucination(target)) {
				kill_unit(target);
				continue;
			}
			if (target == source_unit) continue;
			if (u_invincible(target)) continue;
			if (u_burrowed(target)) continue;
			if (ut_building(target)) continue;
			if (!ut_organic(target)) continue;
			maelstrom_unit(target);
		}
	}

	void disruption_web(xy pos, int owner) {
		const unit_type_t* unit_type = get_unit_type(UnitTypes::Spell_Disruption_Web);
		pos = restrict_move_target_to_valid_bounds(unit_type, pos);
		unit_t* u = create_unit(unit_type, pos, 11);
		if (u) {
			u_set_status_flag(u, unit_t::status_flag_no_collide);
			u->sprite->elevation_level = 11;
			finish_building_unit(u);
			complete_unit(u);
			set_remove_timer(u, 360);
		} else display_last_error_for_player(owner);
	}

	ImageTypes acid_spore_image(const unit_t* u) const {
		size_t n = std::min((size_t)(u->acid_spore_count / 2), (size_t)3);
		return (ImageTypes)((int)ImageTypes::IMAGEID_Acid_Spores_1_Overlay_Small + 4 * unit_sprite_size(u) + n);
	}

	void update_acid_spore_image(unit_t* u) {
		ImageTypes image_id = acid_spore_image(u);
		if (!find_image(u, image_id, image_id)) {
			destroy_image_from_to(u, ImageTypes::IMAGEID_Acid_Spores_1_Overlay_Small, ImageTypes::IMAGEID_Acid_Spores_6_9_Overlay_Large);
			create_image(get_image_type(image_id), (u->subunit ? u->subunit : u)->sprite, {}, image_order_top);
		}
	}

	void add_acid_spore(unit_t* u) {
		if (u->acid_spore_count < 9) ++u->acid_spore_count;
		update_acid_spore_image(u);
		*get_best_score(u->acid_spore_time, identity()) = 150;
	}

	void add_acid_spore(xy pos, int owner) {
		for (unit_t* target : find_units_noexpand(square_at(pos, 64))) {
			if (target->owner == owner) continue;
			if (ut_building(target)) continue;
			if (!u_flying(target)) continue;
			if (u_invincible(target)) continue;
			if (unit_is_undetected(target, owner)) continue;
			add_acid_spore(target);
		}
	}

	void bullet_hit(bullet_t* b) {
		switch (b->weapon_type->hit_type) {
		case weapon_type_t::hit_type_none:
			break;
		case weapon_type_t::hit_type_normal:
			if (b->bullet_target && ~b->hit_flags & 1) {
				int div = 1;
				if (b->weapon_type->bullet_type == weapon_type_t::bullet_type_bounce) {
					int bounces = 2 - b->remaining_bounces;
					for (int i = 0; i < bounces; ++i) {
						div *= 3;
					}
				}
				bullet_deal_damage(b, b->bullet_target, div);
			}
			break;
		case weapon_type_t::hit_type_radial_splash:
		case weapon_type_t::hit_type_enemy_splash:
		case weapon_type_t::hit_type_nuclear_missile:
			if (b->weapon_type->id == WeaponTypes::Subterranean_Spines) {
				for (unit_t* target : find_units(square_at(b->sprite->position, b->weapon_type->outer_splash_radius))) {
					if (target == b->bullet_owner_unit) continue;
					if (target->owner == b->owner) continue;
					if (!weapon_can_target_unit(b->weapon_type, target)) continue;
					if (unit_distance_to(target, b->sprite->position) > b->weapon_type->inner_splash_radius) continue;
					if (b->bullet_owner_unit) {
						bool found = false;
						for (auto& arr : st.recent_lurker_hits) {
							if (std::find(arr.begin(), arr.end(), std::make_pair(b->bullet_owner_unit->index, target->index)) != arr.end()) {
								found = true;
								break;
							}
						}
						if (found) continue;
						auto& arr = st.recent_lurker_hits[st.recent_lurker_hit_current_index];
						if (arr.size() != 16) {
							arr.emplace_back(b->bullet_owner_unit->index, target->index);
						}
					}
					bullet_deal_damage(b, target);
				}
			} else {
				bullet_deal_splash_damage<false>(b);
			}
			break;
		case weapon_type_t::hit_type_lockdown:
			if (b->bullet_target && !unit_dying(b->bullet_target)) lockdown_unit(b->bullet_target);
			break;
		case weapon_type_t::hit_type_parasite:
			if (b->bullet_target && !unit_dying(b->bullet_target)) {
				play_sound(921, b->bullet_target);
				parasite_unit(b->bullet_target, b->owner);
			}
			break;
		case weapon_type_t::hit_type_broodlings:
			if (b->bullet_target && b->bullet_owner_unit && !unit_dying(b->bullet_target)) spawn_broodlings(b->bullet_target, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_emp_shockwave:
			emp_shockwave(b->sprite->position, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_irradiate:
			if (b->bullet_target && !unit_dying(b->bullet_target)) {
				play_sound(351, b->bullet_target);
				irradiate_unit(b->bullet_target, b->bullet_owner_unit, b->owner);
			}
			break;
		case weapon_type_t::hit_type_ensnare:
			ensnare(b->sprite->position, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_plague:
			plague(b->bullet_target_pos, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_stasis_field:
			if (b->bullet_target_pos != xy()) stasis_field(b->bullet_target_pos, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_dark_swarm:
			dark_swarm(b->bullet_target_pos, b->owner);
			break;
		case weapon_type_t::hit_type_yamato_gun:
			if (b->bullet_target) bullet_deal_damage(b, b->bullet_target);
			break;
		case weapon_type_t::hit_type_restoration:
			if (b->bullet_target) {
				play_sound(998, b->bullet_target);
				restore_unit(b->bullet_target);
			}
			break;
		case weapon_type_t::hit_type_optical_flare:
			if (b->bullet_target && !unit_dying(b->bullet_target)) blind_unit(b->bullet_target, b->owner);
			break;
		case weapon_type_t::hit_type_air_splash:
			bullet_deal_splash_damage<true>(b);
			break;
		case weapon_type_t::hit_type_consume:
			if (b->bullet_target && b->bullet_owner_unit && !unit_dying(b->bullet_target)) consume_unit(b->bullet_target, b->bullet_owner_unit);
			break;
		case weapon_type_t::hit_type_disruption_web:
			disruption_web(b->bullet_target_pos, b->owner);
			break;
		case weapon_type_t::hit_type_corrosive_acid:
			if (b->bullet_target && ~b->hit_flags & 1) {
				bullet_deal_damage(b, b->bullet_target);
				if (~b->hit_flags & 2) {
					add_acid_spore(b->sprite->position, b->owner);
				}
			}
			break;
		case weapon_type_t::hit_type_maelstrom:
			if (b->bullet_target_pos != xy()) maelstrom(b->bullet_target_pos, b->bullet_owner_unit);
			break;
		default: error("unknown bullet hit type %d", b->weapon_type->hit_type);
		}
	}

	void bullet_kill(bullet_t* b) {
		b->bullet_state = bullet_t::state_dying;
		sprite_run_anim(b->sprite, iscript_anims::Death);
	}

	void bullet_move(bullet_t* b, execute_movement_struct& ems) {
		update_unit_movement_values(b, ems);
		finish_flingy_movement(b, ems);
		if (!is_in_bounds(b->position, map_bounds())) b->remaining_time = 0;
		xy pos = restrict_pos_to_map_bounds(b->position);
		move_sprite(b->sprite, pos);
		if (b->position != pos) {
			b->position = pos;
			b->exact_position = to_xy_fp8(pos);
		}
	}

	void set_bullet_move_target(bullet_t* b, xy target) {
		set_next_target_waypoint(b, target);
		set_flingy_move_target(b, target);
	}

	bool bullet_state_init(bullet_t* b, execute_movement_struct& ems) {
		if (~b->order_signal & 1) return false;
		b->order_signal &= ~1;
		switch (b->weapon_type->bullet_type) {
		case weapon_type_t::bullet_type_fly:
		case weapon_type_t::bullet_type_extend_to_max_range:
			b->bullet_state = bullet_t::state_move;
			sprite_run_anim(b->sprite, iscript_anims::GndAttkInit);
			break;
		case weapon_type_t::bullet_type_appear_at_target_unit:
		case weapon_type_t::bullet_type_appear_at_target_pos:
		case weapon_type_t::bullet_type_appear_at_source_unit:
		case weapon_type_t::bullet_type_self_destruct:
			bullet_kill(b);
			break;
		case weapon_type_t::bullet_type_follow_target:
			b->bullet_state = bullet_t::state_follow;
			sprite_run_anim(b->sprite, iscript_anims::GndAttkInit);
			break;
		case weapon_type_t::bullet_type_persist_at_target_pos:
			b->bullet_state = bullet_t::state_damage_over_time;
			sprite_run_anim(b->sprite, iscript_anims::SpecialState2);
			break;
		case weapon_type_t::bullet_type_bounce:
			b->bullet_state = bullet_t::state_bounce;
			sprite_run_anim(b->sprite, iscript_anims::GndAttkInit);
			break;
		case weapon_type_t::bullet_type_attack_target_pos:
			b->bullet_state = bullet_t::state_hit_near_target;
			sprite_run_anim(b->sprite, iscript_anims::GndAttkInit);
			break;
		default: error("unknown bullet type %d", b->weapon_type->bullet_type);
		}
		return true;
	}

	bool bullet_state_dying(bullet_t* b, execute_movement_struct& ems) {
		if (b->sprite) return false;
		--st.active_bullets_size;
		st.active_bullets.remove(*b);
		st.bullets_container.push(b);
		return false;
	}

	bool bullet_state_move(bullet_t* b, execute_movement_struct& ems) {
		bullet_move(b, ems);
		if (b->remaining_time-- == 0 || b->position == b->move_target.pos) {
			bullet_kill(b);
		}
		return false;
	}

	bool bullet_state_follow(bullet_t* b, execute_movement_struct& ems) {
		unit_t* target = b->bullet_target;
		if (target) {
			if (~b->hit_flags & 1) set_bullet_move_target(b, target->sprite->position);
			else b->bullet_state = bullet_t::state_move;
		} else {
			b->bullet_state = bullet_t::state_move;
			b->bullet_target = nullptr;
		}
		return bullet_state_move(b, ems);
	}

	bool bullet_state_hit_near_target(bullet_t* b, execute_movement_struct& ems) {
		unit_t* target = b->bullet_target;
		if (target) {
			if (~b->hit_flags & 1) {
				set_flingy_move_target(b, restrict_pos_to_map_bounds(target->sprite->position + hit_near_target_positions[b->hit_near_target_position_index]));
				set_next_target_waypoint(b, b->move_target.pos);
			} else b->bullet_state = bullet_t::state_move;
		} else {
			b->bullet_state = bullet_t::state_move;
			b->bullet_target = nullptr;
		}
		return bullet_state_move(b, ems);
	}

	bool bullet_state_bounce(bullet_t* b, execute_movement_struct& ems) {
		unit_t* target = b->bullet_target;
		if (target && ~b->hit_flags & 1) {
			set_flingy_move_target(b, target->sprite->position);
			set_next_target_waypoint(b, b->move_target.pos);
		}
		bullet_move(b, ems);
		if (unit_is_at_move_target(b)) {
			--b->remaining_bounces;
			unit_t* new_target = nullptr;
			if (b->remaining_bounces) {
				if (b->bullet_owner_unit) {
					new_target = find_unit_noexpand(square_at(b->sprite->position, 96), [&](unit_t* u) {
						if (!unit_can_attack_target(b->bullet_owner_unit, u)) return false;
						if (!unit_target_is_enemy(b->bullet_owner_unit, u)) return false;
						if (u == target || u == b->prev_bounce_unit) return false;
						return true;
					});
				}
				b->prev_bounce_unit = target;
			}
			if (new_target) {
				sprite_run_anim(b->sprite, iscript_anims::SpecialState1);
				b->bullet_target = new_target;
			} else {
				bullet_kill(b);
			}
		}
		return false;
	}

	bool bullet_state_damage_over_time(bullet_t* b, execute_movement_struct& ems) {
		if (b->remaining_time-- == 0) {
			bullet_kill(b);
		} else {
			if (b->remaining_time % 7 == 0) bullet_hit(b);
		}
		return false;
	}

	void bullet_execute(bullet_t* b) {
		execute_movement_struct ems;
		while (true) {
			bool cont = false;
			switch (b->bullet_state) {
			case bullet_t::state_init:
				cont = bullet_state_init(b, ems);
				break;
			case bullet_t::state_move:
				cont = bullet_state_move(b, ems);
				break;
			case bullet_t::state_follow:
				cont = bullet_state_follow(b, ems);
				break;
			case bullet_t::state_bounce:
				cont = bullet_state_bounce(b, ems);
				break;
			case bullet_t::state_damage_over_time:
				cont = bullet_state_damage_over_time(b, ems);
				break;
			case bullet_t::state_dying:
				cont = bullet_state_dying(b, ems);
				break;
			case bullet_t::state_hit_near_target:
				cont = bullet_state_hit_near_target(b, ems);
				break;
			default: error("unknown bullet state %d", b->bullet_state);
			}
			if (!cont) break;
		}
	}

	bool unit_is_under_dark_swarm(const unit_t* u) const {
		if (ut_building(u)) return false;
		if (st.completed_unit_counts[11][UnitTypes::Spell_Dark_Swarm] == 0) return false;
		return find_unit(unit_sprite_inner_bounding_box(u), [&](const unit_t* n) {
			return unit_is(n, UnitTypes::Spell_Dark_Swarm);
		}) != nullptr;
	}

	fp8 unit_dodge_chance(const unit_t* u) const {
		if (u_flying(u)) return 0_fp8;
		if (unit_is_under_dark_swarm(u)) return 255_fp8;
		if (st.tiles[tile_index(u->sprite->position)].flags & tile_t::flag_provides_cover) return 119_fp8;
		return 0_fp8;
	}

	fp8 unit_target_miss_chance(const unit_t* u, const unit_t* target) const {
		fp8 r = unit_dodge_chance(target);
		if (!u_flying(u) && !u_flying(target)) {
			if (get_ground_height_at(target->sprite->position) > get_ground_height_at(u->sprite->position)) {
				if (r < 119_fp8) r = 119_fp8;
			}
		}
		return r;
	}

	bool initialize_bullet(bullet_t* b, const weapon_type_t* weapon_type, unit_t* source_unit, xy pos, int owner, direction_t heading) {
		const flingy_type_t* flingy_type = weapon_type->flingy;
		if (!flingy_type) error("attempt to create bullet with null flingy");
		if (!initialize_flingy(b, flingy_type, pos, owner, heading)) return false;

		b->movement_flags |= 8;
		b->bullet_state = 0;
		b->bullet_target = nullptr;
		b->order_signal = 0;
		b->weapon_type = weapon_type;
		b->remaining_time = weapon_type->lifetime;
		b->hit_flags = 0;
		b->remaining_bounces = 0;
		b->owner = owner;

		if (weapon_type->bullet_heading_offset != 0_dir) {
			bool clockwise;
			if (source_unit == st.prev_bullet_source_unit) clockwise = !st.prev_bullet_heading_offset_clockwise;
			else clockwise = lcg_rand(0) & 1;
			direction_t heading_offset = weapon_type->bullet_heading_offset;
			if (!clockwise) heading_offset = -heading_offset;
			b->next_velocity_direction += heading_offset;
			b->heading = b->next_velocity_direction;
			st.prev_bullet_source_unit = source_unit;
			st.prev_bullet_heading_offset_clockwise = clockwise;
		}

		unit_t* bullet_owner_unit = source_unit;
		if (ut_turret(source_unit)) bullet_owner_unit = source_unit->subunit;
		if (unit_is(source_unit, UnitTypes::Protoss_Scarab)) bullet_owner_unit = source_unit->fighter.parent;
		b->bullet_owner_unit = bullet_owner_unit;

		if (u_hallucination(source_unit)) b->hit_flags |= 2;
		b->prev_bounce_unit = nullptr;

		unit_t* target_unit = source_unit->order_target.unit;
		xy target_pos;
		if (target_unit) {
			b->sprite->elevation_level = target_unit->sprite->elevation_level + 1;
			b->bullet_target = target_unit;
			target_pos = target_unit->sprite->position;
		} else {
			b->sprite->elevation_level = source_unit->sprite->elevation_level + 1;
			b->bullet_target = nullptr;
			target_pos = source_unit->order_target.pos;
		}
		switch (weapon_type->bullet_type) {
		case weapon_type_t::bullet_type_fly:
		case weapon_type_t::bullet_type_follow_target:
		case weapon_type_t::bullet_type_bounce:
			if (weapon_type->bullet_type == weapon_type_t::bullet_type_bounce) b->remaining_bounces = 3;
			if (target_unit && bullet_owner_unit && fp8::from_raw(lcg_rand(1) & 0xff) <= unit_target_miss_chance(bullet_owner_unit, target_unit)) {
				b->hit_flags |= 1;
				target_pos -= to_xy(direction_xy(heading, 30));
			}
			set_flingy_move_target(b, target_pos);
			break;
		case weapon_type_t::bullet_type_appear_at_target_unit:
		case weapon_type_t::bullet_type_appear_at_target_pos:
			if (target_unit && bullet_owner_unit && fp8::from_raw(lcg_rand(1) & 0xff) <= unit_target_miss_chance(bullet_owner_unit, target_unit)) {
				b->hit_flags |= 1;
				xy pos = b->sprite->position - to_xy(direction_xy(b->heading, 30));
				b->exact_position = to_xy_fp8(pos);
				b->position = pos;
				move_sprite(b->sprite, pos);
			}
			break;
		case weapon_type_t::bullet_type_persist_at_target_pos:
			b->exact_position = to_xy_fp8(target_pos);
			b->position = target_pos;
			move_sprite(b->sprite, target_pos);
			break;
		case weapon_type_t::bullet_type_appear_at_source_unit:
			break;
		case weapon_type_t::bullet_type_self_destruct:
			if (b->bullet_owner_unit) {
				u_set_status_flag(b->bullet_owner_unit, unit_t::status_flag_lifetime_expired);
				b->bullet_owner_unit->user_action_flags |= 4;
				kill_unit(b->bullet_owner_unit);
			}
			break;
		case weapon_type_t::bullet_type_attack_target_pos:
			b->hit_near_target_position_index = source_unit->next_hit_near_target_position_index;
			if (source_unit->next_hit_near_target_position_index == 13) source_unit->next_hit_near_target_position_index = 0;
			else ++source_unit->next_hit_near_target_position_index;
			set_flingy_move_target(b, target_pos + hit_near_target_positions[b->hit_near_target_position_index]);
			break;
		case weapon_type_t::bullet_type_extend_to_max_range:
			target_pos = source_unit->sprite->position + to_xy(direction_xy(b->heading, b->weapon_type->max_range + 20));
			set_flingy_move_target(b, target_pos);
			break;
		default: error("unknown bullet_type %d", weapon_type->bullet_type);
		}
		b->bullet_target_pos = target_pos;
		return true;
	}

	bullet_t* create_bullet(const weapon_type_t* weapon_type, unit_t* source_unit, xy pos, int owner, direction_t heading) {
		if (weapon_type->id == WeaponTypes::Halo_Rockets && st.active_bullets_size >= 80) return nullptr;
		if (u_cannot_attack(source_unit) && !is_spell(weapon_type)) return nullptr;
		bullet_t* b = st.bullets_container.top();
		if (!b) return nullptr;
		if (!initialize_bullet(b, weapon_type, source_unit, pos, owner, heading)) {
			return nullptr;
		}
		st.bullets_container.pop();
		++st.active_bullets_size;
		bw_insert_list(st.active_bullets, *b);
		return b;
	}

	xy get_bullet_appear_at_target_pos(const unit_t* u, const unit_t* target) const {
		auto target_bb = unit_sprite_inner_bounding_box(u->order_target.unit);
		int margin_w = (target_bb.to.x - target_bb.from.x) / 4;
		int margin_h = (target_bb.to.y - target_bb.from.y) / 4;
		rect bb = target_bb + rect{{margin_w, margin_h}, {-margin_w, -margin_h}};
		xy a = bb.from + xy((bb.to.x - bb.from.x) / 2, (bb.to.y - bb.from.y) / 2);
		xy b = u->sprite->position;
		if (get_unique_sided_positions_within_bounds(a, b, bb)) return b;
		else return a;
	}

	void fire_weapon(unit_t* u, const weapon_type_t* weapon_type, int forward_offset = -1) {
		if (!weapon_type->flingy) return;
		xy pos;
		if (weapon_type->bullet_type == weapon_type_t::bullet_type_appear_at_target_unit) {
			if (!u->order_target.unit) return;
			pos = get_bullet_appear_at_target_pos(u, u->order_target.unit);
		} else {
			if (weapon_type->bullet_type == weapon_type_t::bullet_type_appear_at_target_pos) {
				pos = u->order_target.pos;
			} else {
				pos = u->sprite->position + to_xy(direction_xy(u->heading, forward_offset == -1 ? weapon_type->forward_offset : forward_offset));
				pos.y -= weapon_type->upward_offset;
			}
		}
		create_bullet(weapon_type, u, pos, u->owner, u->heading);
	}

	bool unit_tech_target_valid(const unit_t* u, const tech_type_t* tech, const unit_t* target) const {
		if (target->stasis_timer) return false;
		switch (tech->id) {
		case TechTypes::Feedback:
			if (ut_building(target)) return false;
			if (!ut_has_energy(target)) return false;
			if (u_hallucination(target)) return false;
			return true;
		case TechTypes::Mind_Control:
			if (target->owner == u->owner) return false;
			if (ut_building(target)) return false;
			if (unit_is(target, UnitTypes::Terran_Vulture_Spider_Mine)) return false;
			if (unit_is(target, UnitTypes::Zerg_Larva)) return false;
			if (unit_is_egg(target)) return false;
			if (unit_is(target, UnitTypes::Protoss_Interceptor)) return false;
			if (unit_is(target, UnitTypes::Protoss_Scarab)) return false;
			return true;
		case TechTypes::Hallucination:
			if (ut_building(target)) return false;
			if (unit_is(target, UnitTypes::Protoss_Interceptor)) return false;
			return true;
		case TechTypes::Defensive_Matrix:
		case TechTypes::Irradiate:
		case TechTypes::Restoration:
		case TechTypes::Optical_Flare:
			if (ut_building(target)) return false;
			return true;
		case TechTypes::Consume:
			if (ut_building(target)) return false;
			if (target->owner != u->owner) return false;
			if (unit_race(target) == race_t::terran) return false;
			if (unit_is(target, UnitTypes::Zerg_Larva)) return false;
			return true;
		default:
			return false;
		}
	}

	bool spell_order_valid(const unit_t* u) const {
		if (u->order_type->targets_enemies) {
			if (iscript_unit->order_type->weapon == WeaponTypes::None) return false;
			return weapon_can_target_unit(get_weapon_type(iscript_unit->order_type->weapon), u->order_target.unit, u);
		}
		switch (u->order_type->tech_type) {
		case TechTypes::None:
			return true;
		case TechTypes::Spider_Mines:
			return (st.tiles[tile_index(u->order_target.pos)].flags & (tile_t::flag_walkable | tile_t::flag_has_creep)) != 0;
		case TechTypes::EMP_Shockwave:
		case TechTypes::Scanner_Sweep:
		case TechTypes::Dark_Swarm:
		case TechTypes::Plague:
		case TechTypes::Ensnare:
		case TechTypes::Psionic_Storm:
		case TechTypes::Recall:
		case TechTypes::Stasis_Field:
		case TechTypes::Disruption_Web:
		case TechTypes::Maelstrom:
			return true;
		default:
			if (!u->order_target.unit || u_invincible(u->order_target.unit)) return false;
			return unit_tech_target_valid(u, get_tech_type(u->order_type->tech_type), u->order_target.unit);
		}
	}

