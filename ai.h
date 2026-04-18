#ifndef BWGAME_AI_H
#define BWGAME_AI_H

#include "bwgame.h"

namespace bwgame {

struct ai_functions : state_functions {
	ai_state& ai_st;
	explicit ai_functions(state& st) : state_functions(st), ai_st(st.ai_st) {}
	virtual void trigger_start_ai_script(int player, uint32_t tag, xy pos = {}, unit_t* u = nullptr) override {
		start_script(player, tag, pos, u);
	}
	virtual void update_ai() override {
		for (int p = 0; p < 8; ++p) {
			for (auto it = ai_st.player_scripts[p].begin(); it != ai_st.player_scripts[p].end();) {
				if (!it->active) {
					it = ai_st.player_scripts[p].erase(it);
					continue;
				}
				if (it->wait_timer > 0) {
					--it->wait_timer;
					++it;
					continue;
				}
				execute_script(*it);
				++it;
			}
		}
	}

	void execute_script(ai_script_t& s) {
		auto& bin = st.global->aiscript_bin;
		if (s.offset == 0 || s.offset >= bin.size()) { s.active = false; return; }
		
		const uint8_t* base = bin.data();
		while (s.active && s.wait_timer == 0) {
			if (s.offset >= bin.size()) { s.active = false; break; }
			uint8_t opcode = base[s.offset++];
			switch (opcode) {
				case 0x00: // wait
					if (s.offset + 1 > bin.size()) { s.active = false; break; }
					s.wait_timer = *(uint8_t*)(base + s.offset);
					s.offset += 1;
					break;
				case 0x01: // start_town
					break;
				case 0x02: // multi_run
					if (s.offset + 2 > bin.size()) { s.active = false; break; }
					// Run another script at offset
					start_script(s.player, *(uint16_t*)(base + s.offset), s.location, s.center_unit);
					s.offset += 2;
					break;
				case 0x03: // expand
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					s.offset += 3;
					break;
				case 0x05: // goto
					if (s.offset + 2 > bin.size()) { s.active = false; break; }
					s.offset = *(uint16_t*)(base + s.offset);
					break;
				case 0x07: // wait_build
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					{
						uint16_t unit_id = *(uint16_t*)(base + s.offset);
						uint8_t count = *(uint8_t*)(base + s.offset + 2);
						s.offset += 3;
						if (st.completed_unit_counts[s.player].at((UnitTypes)unit_id) < count) {
							s.offset -= 4; // Re-execute wait_build
							s.wait_timer = 30; // Wait 30 frames before checking again
						}
					}
					break;
				case 0x08: // attack_to
					if (s.offset + 4 > bin.size()) { s.active = false; break; }
					{
						uint16_t x = *(uint16_t*)(base + s.offset);
						uint16_t y = *(uint16_t*)(base + s.offset + 2);
						s.offset += 4;
						xy pos(x, y);
						for (unit_t& u : st.player_units[s.player]) {
							if (unit_dead(&u) || ut_building(&u) || ut_worker(&u) || !u_can_move(&u)) continue;
							if (u.order_type->id == Orders::ComputerAI || u.order_type->id == u.unit_type->human_ai_idle->id) {
								set_unit_order(&u, get_order_type(Orders::AttackMove), pos);
							}
						}
					}
					break;
				case 0x0D: // build
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					s.offset += 3;
					break;
				case 0x0E: // help_build
				case 0x0F: // look_for_towns
					break;
				case 0x10: // farms_notiming
				case 0x11: // farms_timing
				case 0x12: // build_ready
					break;
				case 0x13: // set_ignore
					if (s.offset + 1 > bin.size()) { s.active = false; break; }
					s.offset += 1;
					break;
				case 0x14: // res_area
				case 0x15: // guard_resources
					break;
				case 0x16: // settler
					break;
				case 0x1B: // if_unit
					if (s.offset + 4 > bin.size()) { s.active = false; break; }
					{
						uint16_t unit_id = *(uint16_t*)(base + s.offset);
						uint16_t label = *(uint16_t*)(base + s.offset + 2);
						s.offset += 4;
						if (st.unit_counts[s.player].at((UnitTypes)unit_id) > 0)
							s.offset = label;
					}
					break;
				case 0x1C: // skip_if_unit
					if (s.offset + 4 > bin.size()) { s.active = false; break; }
					{
						uint16_t unit_id = *(uint16_t*)(base + s.offset);
						uint16_t label = *(uint16_t*)(base + s.offset + 2);
						s.offset += 4;
						if (st.unit_counts[s.player].at((UnitTypes)unit_id) == 0)
							s.offset = label;
					}
					break;
				case 0x1E: // wait_buildstart
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					{
						uint16_t unit_id = *(uint16_t*)(base + s.offset);
						uint8_t count = *(uint8_t*)(base + s.offset + 2);
						s.offset += 3;
						if (st.unit_counts[s.player].at((UnitTypes)unit_id) < count) {
							s.offset -= 4;
							s.wait_timer = 30;
						}
					}
					break;
				case 0x23: // attack_add
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					s.offset += 3;
					break;
				case 0x24: // attack_prepare
					if (s.offset + 1 > bin.size()) { s.active = false; break; }
					s.offset += 1;
					break;
				case 0x21: // train
					if (s.offset + 2 > bin.size()) { s.active = false; break; }
					{
						uint16_t unit_id = *(uint16_t*)(base + s.offset);
						s.offset += 2;
						ai_train(s.player, (UnitTypes)unit_id);
					}
					break;
				case 0x22: // tech
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					s.offset += 3;
					break;
				case 0x25: // upgrade
					if (s.offset + 4 > bin.size()) { s.active = false; break; }
					s.offset += 4;
					break;
					break;
				default:
					s.active = false;
					break;
			}
		}
	}

	void start_script(int player, uint32_t tag, xy pos = {}, unit_t* u = nullptr) {
		auto& bin = st.global->aiscript_bin;
		if (bin.size() < 4) return;
		
		const uint8_t* base = bin.data();
		size_t count = *(uint32_t*)base;
		const uint8_t* ptr = base + 4;
		
		if (4 + count * 8 > bin.size()) return;

		for (size_t i = 0; i < count; ++i) {
			uint32_t script_tag = *(uint32_t*)ptr;
			uint32_t offset = *(uint32_t*)(ptr + 4);
			if (script_tag == tag) {
				ai_script_t s;
				s.offset = (size_t)offset;
				s.player = player;
				s.location = pos;
				s.center_unit = u;
				s.active = true;
				ai_st.player_scripts[player].push_back(std::move(s));
				return;
			}
			ptr += 8;
		}
	}
};

} // namespace bwgame

#endif
