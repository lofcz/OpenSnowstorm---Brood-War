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
				case 0x0D: // build
					if (s.offset + 3 > bin.size()) { s.active = false; break; }
					s.offset += 3;
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
						if (s.center_unit) {
							// For simplicity, we use the center unit as the trainer if it's the right type
							// In full fidelity, the AI player searches for a trainer.
							ai_train(s.player, (UnitTypes)unit_id);
						}
					}
					break;
				case 0x33: // stop
					s.active = false;
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
