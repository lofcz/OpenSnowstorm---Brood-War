#ifndef BWGAME_PATHFINDING_H
#define BWGAME_PATHFINDING_H


	struct pathfinder {
		const unit_t* u = nullptr;
		const unit_t* target_unit = nullptr;
		xy source;
		const regions_t::region* source_region = nullptr;
		const regions_t::region* destination_region = nullptr;
		rect unit_bb;
		rect target_unit_bb;
		xy destination;

		a_circular_vector<const regions_t::region*> long_path;
		size_t full_long_path_size;
		a_circular_vector<xy> short_path;

		size_t current_long_path_index = 0;
		size_t current_short_path_index = 0;

		size_t short_highest_open_size = 0;
		size_t short_all_nodes_size = 0;
		size_t long_highest_open_size = 0;
		size_t long_all_nodes_size = 0;

		bool destination_reached = false;
		bool is_stuck = false;

		const unit_t* consider_collision_with_unit = nullptr;
		bool consider_collision_with_moving_units = false;
	};

	bool pathfinder_find_long_path(pathfinder& pf) const {
		if (pf.source_region == pf.destination_region) return false;

		struct node_t {
			node_t* prev = nullptr;
			xy_fp8 pos;
			const regions_t::region* region = nullptr;
			fp8 total_cost{};
			fp8 estimated_remaining_cost{};
			fp8 estimated_final_cost{};
			bool visited = false;
		};
		struct cmp_node {
			bool operator()(const node_t* a, const node_t* b) const {
				return a->estimated_final_cost < b->estimated_final_cost;
			}
		};
		a_vector<node_t*> open;

		a_list<node_t> all_nodes;

		node_t* goal_node = nullptr;

		auto region_pos = [&](const regions_t::region* r) {
			if (r == pf.source_region) return to_xy_fp8(pf.source);
			if (r == pf.destination_region) return to_xy_fp8(pf.destination);
			return r->center;
		};

		auto find = [&](const regions_t::region* from_region, const regions_t::region* to_region) {

			xy_fp8 to_pos = region_pos(to_region);

			all_nodes.clear();
			open.clear();

			all_nodes.emplace_back();
			node_t* start_node = &all_nodes.back();
			start_node->pos = region_pos(from_region);
			start_node->region = from_region;
			start_node->estimated_remaining_cost = fp8::integer(128 * 128);
			start_node->estimated_final_cost = start_node->estimated_remaining_cost;
			start_node->region->pathfinder_node = (void*)start_node;

			open.push_back(start_node);
			binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());

			while (!open.empty()) {
				node_t* cur = open.front();
				std::swap(open.front(), open.back());
				open.pop_back();
				binary_heap_down(open.begin(), open.begin(), open.end(), cmp_node());

				if (cur->region == to_region) {
					goal_node = cur;
					break;
				}
				cur->visited = true;

				auto add = [&](const regions_t::region* r) {
					if (open.size() == 125) return;
					if (all_nodes.size() == 350) return;
					xy_fp8 pos = region_pos(r);
					fp8 cost = xy_length(pos - cur->pos);
					if (r->group_index != pf.source_region->group_index) {
						cost *= 2;
					}
					fp8 total_cost = cur->total_cost + cost;
					node_t* n = (node_t*)r->pathfinder_node;
					if (!n) {
						all_nodes.emplace_back();
						n = &all_nodes.back();
						n->prev = cur;
						n->pos = pos;
						n->region = r;
						n->total_cost = total_cost;
						n->estimated_remaining_cost = xy_length(to_pos - pos);
						n->estimated_final_cost = n->total_cost + n->estimated_remaining_cost;
						n->visited = false;
						r->pathfinder_node = (void*)n;
						open.push_back(n);
						binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());
					} else if (cur->prev != n) {
						if (total_cost < n->total_cost) {
							n->prev = cur;
							n->total_cost = total_cost;
							fp8 estimated_final_cost = n->total_cost + n->estimated_remaining_cost;
							if (n->visited) {
								n->estimated_final_cost = estimated_final_cost;
								n->visited = false;
								open.push_back(n);
								binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());
							} else {
								if (estimated_final_cost >= n->estimated_final_cost) error("unreachable; cost did not decrease");
								n->estimated_final_cost = estimated_final_cost;
								binary_heap_up(std::find(open.begin(), open.end(), n), open.begin(), open.end(), cmp_node());
							}
						}
					}
				};
				for (auto* n : cur->region->walkable_neighbors) {
					add(n);
				}
				if (cur->region->group_index != pf.source_region->group_index) {
					for (auto* n : cur->region->non_walkable_neighbors) {
						add(n);
					}
				}
				if (open.size() > pf.long_highest_open_size) {
					pf.long_highest_open_size = open.size();
				}
				if (open.size() == 125) break;
				if (all_nodes.size() == 350) break;

			}
			if (!goal_node && !open.empty()) {
				goal_node = open.front();
			}

			if (!goal_node) {
				start_node->estimated_remaining_cost = xy_length(to_pos - start_node->region->center);
				fp8 best_cost = start_node->estimated_remaining_cost;
				node_t* best_node = start_node;
				for (auto i = std::next(all_nodes.begin()); i != all_nodes.end(); ++i) {
					if (i->estimated_remaining_cost < best_cost) {
						best_cost = i->estimated_remaining_cost;
						best_node = &*i;
					}
				}
				goal_node = best_node;
			}

		};

		bool path_is_reversed;
		if (pf.source_region->group_index == pf.destination_region->group_index) {
			find(pf.source_region, pf.destination_region);
			path_is_reversed = false;
		} else {
			find(pf.destination_region, pf.source_region);
			path_is_reversed = true;
			if (goal_node->region != pf.source_region) {
				for (auto& v : all_nodes) {
					v.region->pathfinder_node = nullptr;
				}
				find(pf.source_region, goal_node->region);
				path_is_reversed = false;
			}
		}

		pf.long_all_nodes_size = all_nodes.size();

		pf.long_path.clear();
		pf.full_long_path_size = 0;
		pf.current_long_path_index = (size_t)0 - 1;
		size_t full_path_size = 0;
		if (path_is_reversed) {
			size_t goal_group_index = goal_node->region->group_index;
			for (auto* n = goal_node; n; n = n->prev) {
				if (n->region->group_index != goal_group_index) break;
				if (pf.long_path.size() != 50) pf.long_path.push_back(n->region);
				++full_path_size;
			}
		} else {
			for (auto* n = goal_node; n; n = n->prev) {
				++full_path_size;
			}
			for (size_t i = full_path_size; i > 50; --i) {
				goal_node = goal_node->prev;
			}
			for (auto* n = goal_node; n; n = n->prev) {
				pf.long_path.push_front(n->region);
			}
		}
		pf.full_long_path_size = full_path_size;
		for (auto& v : all_nodes) {
			v.region->pathfinder_node = nullptr;
		}
		return !pf.long_path.empty();
	}

	template<typename random_iterator_T, typename compare_T>
	void binary_heap_up(random_iterator_T element, random_iterator_T begin, random_iterator_T end, compare_T compare) const {
		auto index = element - begin;
		while (element != begin) {
			auto parent_index = (index - 1) / 2;
			auto parent = begin + parent_index;
			if (compare(*parent, *element)) break;
			std::swap(*parent, *element);
			index = parent_index;
			element = parent;
		}
	}
	template<typename random_iterator_T, typename compare_T>
	void binary_heap_down(random_iterator_T element, random_iterator_T begin, random_iterator_T end, compare_T compare) const {
		auto index = element - begin;
		auto end_index = end - begin;
		while (true) {
			auto child_index = index * 2 + 1;
			if (child_index >= end_index) break;
			auto child = begin + child_index;
			if (child + 1 != end) {
				if (compare(*(child + 1), *child)) {
					++child_index;
					++child;
				}
			}
			if (!compare(*child, *element)) break;
			std::swap(*element, *child);
			index = child_index;
			element = child;
		}
	}

	bool pathfinder_unit_can_collide_with(const unit_t* u, const unit_t* target, const unit_t* consider_collision_with_unit, bool consider_collision_with_moving_units) const {
		if (target != consider_collision_with_unit && !consider_collision_with_moving_units) {
			if (!unit_is_at_move_target(target)) return false;
		}
		if (unit_is(target, UnitTypes::Special_Upper_Level_Door)) return false;
		if (unit_is(target, UnitTypes::Special_Right_Upper_Level_Door)) return false;
		if (unit_is(target, UnitTypes::Special_Pit_Door)) return false;
		if (unit_is(target, UnitTypes::Special_Right_Pit_Door)) return false;
		return unit_can_collide_with(u, target);
	}

	bool pathfinder_unit_can_collide_with(const pathfinder& pf, const unit_t* target) const {
		return pathfinder_unit_can_collide_with(pf.u, target, pf.consider_collision_with_unit, pf.consider_collision_with_moving_units);
	}

	void pathfinder_find_short_path(pathfinder& pf, xy target, const regions_t::region* target_region) const {
		bool target_is_destination = target == pf.destination;
		bool target_region_walkable = target_region && target_region->walkable();

		const regions_t::region* source_region = pf.source_region;
		const regions_t::region* destination_region = get_region_at(pf.destination);

		const regions_t::region* move_to_region = target_region ? target_region : destination_region;

		for (auto* nr : move_to_region->walkable_neighbors) {
			if (nr == source_region) continue;
			nr->pathfinder_flag = 1;
		}

		struct pf_search {
			const unit_t* u = nullptr;
			const unit_t* target_unit = nullptr;
			std::array<int, 4> inner;
			std::array<int, 4> outer;
			rect target_unit_bb;
			xy target;

			bool has_found_goal;
			xy cur_pos;
			xy cur_pos_max;
			xy cur_pos_min;

			std::array<a_vector<regions_t::contour>, 4> local_edges;

			std::array<const regions_t::contour*, 4> nearest_edge;

			struct neighbor_t {
				xy pos;
				int flags;
				bool is_goal;
			};
			static_vector<neighbor_t, 32> neighbors;

			a_vector<rect> visited_areas;
		};

		pf_search w;

		w.u = pf.u;
		w.target_unit = pf.target_unit;
		w.target = target;
		w.inner[0] = pf.u->unit_type->dimensions.from.y;
		w.outer[0] = pf.u->unit_type->dimensions.from.y + 1;
		w.inner[1] = -pf.u->unit_type->dimensions.to.x;
		w.outer[1] = -pf.u->unit_type->dimensions.to.x - 1;
		w.inner[2] = -pf.u->unit_type->dimensions.to.y;
		w.outer[2] = -pf.u->unit_type->dimensions.to.y - 1;
		w.inner[3] = pf.u->unit_type->dimensions.from.x;
		w.outer[3] = pf.u->unit_type->dimensions.from.x + 1;
		w.target_unit_bb = pf.target_unit_bb;

		struct node_t {
			node_t* prev = nullptr;
			xy pos;
			fp8 total_cost{};
			fp8 estimated_remaining_cost{};
			fp8 estimated_final_cost{};
			bool is_goal = false;
			int depth = 0;
			int directional_flags = 0;
			const regions_t::region* region = nullptr;
			bool is_target_region = false;
			bool is_neighbor_region = false;
			bool visited = false;
		};
		struct cmp_node {
			bool operator()(const node_t* a, const node_t* b) const {
				return a->estimated_final_cost < b->estimated_final_cost;
			}
		};
		static_vector<node_t*, 150> open;

		static_vector<node_t, 250> all_nodes;
		all_nodes.emplace_back();
		node_t* start_node = &all_nodes.back();
		start_node->pos = pf.source;
		start_node->estimated_remaining_cost = fp8::integer(128 * 128);
		start_node->estimated_final_cost = start_node->estimated_remaining_cost;
		start_node->directional_flags = 0xff;
		start_node->region = source_region;

		open.push_back(start_node);
		binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());

		struct visited {
			int x;
			a_vector<std::pair<int, int>> y;
		};

		a_vector<visited> pf_area_visited;
		pf_area_visited.push_back({0, {}});
		pf_area_visited.push_back({(int)game_st.map_width, {}});

		auto pf_remove_visited_flags = [&](xy pos, int flags) {
			auto cur = std::upper_bound(pf_area_visited.begin(), pf_area_visited.end(), pos.x, [&](int a, auto& b) {
				return a < b.x;
			});
			--cur;

			auto i = cur->y.begin();
			for (;i != cur->y.end(); ++i) {
				if (pos.y <= i->second) break;
			}
			if (i == cur->y.end()) return flags;
			int mask = 0;
			if (pos.y <= i->first) mask |= 0x19;
			if (pos.y >= i->second) mask |= 0x46;
			if (pos.x == cur->x) {
				mask |= 0x8c;
				if (cur != pf_area_visited.begin()) {
					auto prev = std::prev(cur);
					i = prev->y.begin();
					for (;i != prev->y.end(); ++i) {
						if (pos.y <= i->second) break;
					}
					if (i != prev->y.end()) {
						if (pos.y >= i->first && pos.y <= i->second) {
							mask &= 0x7f;
							if (pos.y > i->first) mask &= 0xf7;
							if (pos.y < i->second) mask &= 0xfb;
						}
					}
				}
			}
			auto next = std::next(cur);
			if (pos.x == next->x - 1) {
				mask |= 0x23;
				i = next->y.begin();
				for (;i != next->y.end(); ++i) {
					if (pos.y <= i->second) break;
				}
				if (i != next->y.end()) {
					if (pos.y >= i->first && pos.y <= i->second) {
						mask &= 0xdf;
						if (pos.y > i->first) mask &= 0xfe;
						if (pos.y < i->second) mask &= 0xfd;
					}
				}
			}
			return flags & mask;
		};

		auto merge_local_edge = [&](auto i, const auto& c) {
			if (c.v[1] + w.inner[c.flags & 3] <= i->v[2] + w.inner[(i->flags >> 2) & 3]) {
				if (c.v[2] + w.inner[(c.flags >> 2) & 3] >= i->v[1] + w.inner[i->flags & 3] - 1) {
					if (c.v[1] < i->v[1]) i->v[1] = c.v[1];
					if (c.v[2] > i->v[2]) i->v[2] = c.v[2];
					return true;
				}
			}
			return false;
		};

		auto pf_add_local_edge = [&](int n, const regions_t::contour& c) {
			auto& local_edges = w.local_edges[n];
			auto cmp_l = [&](auto& a, int b) {
				return a.v[0] < b;
			};
			auto i = std::lower_bound(local_edges.begin(), local_edges.end(), c.v[0], cmp_l);
			while (i != local_edges.end() && i->v[0] == c.v[0]) {
				if (merge_local_edge(i, c)) return;
				if (c.v[1] + w.inner[c.flags & 3] <= i->v[2] + w.inner[(i->flags >> 2) & 3]) break;
				++i;
			}
			local_edges.insert(i, c);
		};

		auto pf_push_back_local_edge = [&](int n, const regions_t::contour& c) {
			auto& local_edges = w.local_edges[n];
			if (!local_edges.empty() && local_edges.back().v[0] == c.v[0]) {
				if (merge_local_edge(std::prev(local_edges.end()), c)) return;
				auto i = std::prev(local_edges.end());
				if (c.v[1] + w.inner[c.flags & 3] <= i->v[2] + w.inner[(i->flags >> 2) & 3]) local_edges.insert(i, c);
				else local_edges.push_back(c);
			} else local_edges.push_back(c);
		};

		auto pf_add_local_terrain = [&]() {
			auto cmp_l = [&](const regions_t::contour& c, int v) {
				return c.v[0] < v;
			};
			auto cmp_u = [&](int v, const regions_t::contour& c) {
				return v < c.v[0];
			};
			auto& c0 = game_st.regions.contours[0];
			for (auto i = std::lower_bound(c0.begin(), c0.end(), w.cur_pos_min.y - w.inner[0] - 1, cmp_l); i != c0.end(); ++i) {
				if (i->v[0] > w.cur_pos.y - w.inner[0] - 1) break;
				if (i->v[1] + w.inner[i->flags & 3] <= w.cur_pos_max.x) {
					if (i->v[2] + w.inner[(i->flags >> 2) & 3] >= w.cur_pos_min.x) {
						pf_push_back_local_edge(0, *i);
					}
				}
			}
			auto& c1 = game_st.regions.contours[1];
			for (auto i = std::upper_bound(c1.begin(), c1.end(), w.cur_pos.x - w.inner[1], cmp_u); i != c1.end(); ++i) {
				if (i->v[0] > w.cur_pos_max.x - w.inner[1] + 1) break;
				if (i->v[1] + w.inner[i->flags & 3] <= w.cur_pos_max.y) {
					if (i->v[2] + w.inner[(i->flags >> 2) & 3] >= w.cur_pos_min.y) {
						pf_push_back_local_edge(1, *i);
					}
				}
			}
			auto& c2 = game_st.regions.contours[2];
			for (auto i = std::upper_bound(c2.begin(), c2.end(), w.cur_pos.y - w.inner[2], cmp_u); i != c2.end(); ++i) {
				if (i->v[0] > w.cur_pos_max.y - w.inner[2] + 1) break;
				if (i->v[1] + w.inner[i->flags & 3] <= w.cur_pos_max.x) {
					if (i->v[2] + w.inner[(i->flags >> 2) & 3] >= w.cur_pos_min.x) {
						pf_push_back_local_edge(2, *i);
					}
				}
			}
			auto& c3 = game_st.regions.contours[3];
			for (auto i = std::lower_bound(c3.begin(), c3.end(), w.cur_pos_min.x - w.inner[3] - 1, cmp_l); i != c3.end(); ++i) {
				if (i->v[0] > w.cur_pos.x - w.inner[3] - 1) break;
				if (i->v[1] + w.inner[i->flags & 3] <= w.cur_pos_max.y) {
					if (i->v[2] + w.inner[(i->flags >> 2) & 3] >= w.cur_pos_min.y) {
						pf_push_back_local_edge(3, *i);
					}
				}
			}
		};

		auto pf_add_local_units = [&]() {
			auto cmp_l = [&](auto& a, int b) {
				return a.value < b;
			};
			for (auto i = std::lower_bound(st.unit_finder_y.begin(), st.unit_finder_y.end(), w.cur_pos_min.y - w.inner[0] - 1, cmp_l); i != st.unit_finder_y.end(); ++i) {
				auto& bb = i->u->unit_finder_bounding_box;
				if (i->value >= w.cur_pos.y - w.inner[0]) break;
				if (i->value == bb.to.y) {
					regions_t::contour c;
					c.v[0] = bb.to.y;
					c.v[1] = bb.from.x;
					c.v[2] = bb.to.x;
					c.dir = 0;
					c.flags = 0x3d;
					if (c.v[1] + w.inner[1] <= w.cur_pos_max.x && c.v[2] + w.inner[3] >= w.cur_pos_min.x) {
						if (i->u == w.target_unit || pathfinder_unit_can_collide_with(pf, i->u)) {
							pf_add_local_edge(0, c);
						}
					}
				}
			}
			for (auto i = std::lower_bound(st.unit_finder_x.begin(), st.unit_finder_x.end(), w.cur_pos.x - w.inner[1], cmp_l); i != st.unit_finder_x.end(); ++i) {
				auto& bb = i->u->unit_finder_bounding_box;
				if (i->value > w.cur_pos_max.x - w.inner[1] + 1) break;
				if (i->value == bb.from.x) {
					regions_t::contour c;
					c.v[0] = bb.from.x;
					c.v[1] = bb.from.y;
					c.v[2] = bb.to.y;
					c.dir = 1;
					c.flags = 0x32;
					if (c.v[1] + w.inner[2] <= w.cur_pos_max.y && c.v[2] + w.inner[0] >= w.cur_pos_min.y) {
						if (i->u == w.target_unit || pathfinder_unit_can_collide_with(pf, i->u)) {
							pf_add_local_edge(1, c);
						}
					}
				}
			}
			for (auto i = std::lower_bound(st.unit_finder_y.begin(), st.unit_finder_y.end(), w.cur_pos.y - w.inner[2], cmp_l); i != st.unit_finder_y.end(); ++i) {
				auto& bb = i->u->unit_finder_bounding_box;
				if (i->value > w.cur_pos_max.y - w.inner[2] + 1) break;
				if (i->value == bb.from.y) {
					regions_t::contour c;
					c.v[0] = bb.from.y;
					c.v[1] = bb.from.x;
					c.v[2] = bb.to.x;
					c.dir = 2;
					c.flags = 0x3d;
					if (c.v[1] + w.inner[1] <= w.cur_pos_max.x && c.v[2] + w.inner[3] >= w.cur_pos_min.x) {
						if (i->u == w.target_unit || pathfinder_unit_can_collide_with(pf, i->u)) {
							pf_add_local_edge(2, c);
						}
					}
				}
			}
			for (auto i = std::lower_bound(st.unit_finder_x.begin(), st.unit_finder_x.end(), w.cur_pos_min.x - w.inner[3] - 1, cmp_l); i != st.unit_finder_x.end(); ++i) {
				auto& bb = i->u->unit_finder_bounding_box;
				if (i->value >= w.cur_pos.x - w.inner[3]) break;
				if (i->value == bb.to.x) {
					regions_t::contour c;
					c.v[0] = bb.to.x;
					c.v[1] = bb.from.y;
					c.v[2] = bb.to.y;
					c.dir = 3;
					c.flags = 0x32;
					if (c.v[1] + w.inner[2] <= w.cur_pos_max.y && c.v[2] + w.inner[0] >= w.cur_pos_min.y) {
						if (i->u == w.target_unit || pathfinder_unit_can_collide_with(pf, i->u)) {
							pf_add_local_edge(3, c);
						}
					}
				}
			}
		};

		auto pf_local_edges_find = [&](int dir, int v, int v0, int v1, int v2) -> const regions_t::contour* {

			auto cmp_u = [&](int v, const regions_t::contour& c) {
				return v < c.v[0];
			};
			auto cmp_l = [&](const regions_t::contour& c, int v) {
				return c.v[0] < v;
			};

			if (dir == 0) {
				auto& c0 = w.local_edges[0];
				for (auto i = std::upper_bound(c0.begin(), c0.end(), v - w.inner[0], cmp_u); i != c0.begin();) {
					--i;
					if (i->v[0] + w.inner[0] < v2) break;
					if (std::max(i->v[1] + w.inner[i->flags & 3], v0) <= std::min(i->v[2] + w.inner[(i->flags >> 2) & 3], v1)) return &*i;
				}
			} else if (dir == 1) {
				auto& c1 = w.local_edges[1];
				for (auto i = std::lower_bound(c1.begin(), c1.end(), v - w.inner[1], cmp_l); i != c1.end(); ++i) {
					if (i->v[0] + w.inner[1] > v2) break;
					if (std::max(i->v[1] + w.inner[i->flags & 3], v0) <= std::min(i->v[2] + w.inner[(i->flags >> 2) & 3], v1)) return &*i;
				}
			} else if (dir == 2) {
				auto& c2 = w.local_edges[2];
				for (auto i = std::lower_bound(c2.begin(), c2.end(), v - w.inner[2], cmp_l); i != c2.end(); ++i) {
					if (i->v[0] + w.inner[2] > v2) break;
					if (std::max(i->v[1] + w.inner[i->flags & 3], v0) <= std::min(i->v[2] + w.inner[(i->flags >> 2) & 3], v1)) return &*i;
				}
			} else if (dir == 3) {
				auto& c3 = w.local_edges[3];
				for (auto i = std::upper_bound(c3.begin(), c3.end(), v - w.inner[3], cmp_u); i != c3.begin();) {
					--i;
					if (i->v[0] + w.inner[3] < v2) break;
					if (std::max(i->v[1] + w.inner[i->flags & 3], v0) <= std::min(i->v[2] + w.inner[(i->flags >> 2) & 3], v1)) return &*i;
				}
			}
			return nullptr;
		};

		auto pf_add_neighbor = [&](xy pos, int flags) {
			if (pos == w.cur_pos) return;
			bool is_goal = false;
			if (w.target_unit) {
				if (pos.x == w.target_unit_bb.from.x || pos.x == w.target_unit_bb.to.x) {
					if (pos.y >= w.target_unit_bb.from.y && pos.y <= w.target_unit_bb.to.y) {
						is_goal = true;
					}
				}
				if (pos.y == w.target_unit_bb.from.y || pos.y == w.target_unit_bb.to.y) {
					if (pos.x >= w.target_unit_bb.from.x && pos.x <= w.target_unit_bb.to.x) {
						is_goal = true;
					}
				}
			}
			if (pos == target) is_goal = true;
			if (is_goal) {
				w.neighbors.push_back({pos, flags, true});
				w.has_found_goal = true;
			} else if (!w.has_found_goal) {
				w.neighbors.push_back({pos, flags, false});
			}
		};

		auto pf_mark_visited = [&](rect bb) {
			int min_x = bb.from.x;
			int max_x = bb.to.x;
			int min_y = bb.from.y;
			int max_y = bb.to.y;

			auto cur = std::upper_bound(pf_area_visited.begin(), pf_area_visited.end(), min_x, [&](int a, auto& b) {
				return a < b.x;
			});
			if (std::prev(cur)->x < min_x) {
				auto& v = *pf_area_visited.insert(cur, {min_x, std::prev(cur)->y});
				cur = std::next(cur);
			}
			auto end = std::upper_bound(cur, pf_area_visited.end(), max_x, [&](int a, auto& b) {
				return a < b.x;
			});
			if (std::prev(end)->x < max_x) {
				end = std::next(pf_area_visited.insert(end, {max_x, std::prev(end)->y}));
			}

			for (; cur != end; ++cur) {
				auto& y = cur->y;
				auto i = std::lower_bound(y.begin(), y.end(), min_y, [&](auto& a, int b) {
					return a.second < b;
				});
				if (i != y.end() && i->first <= max_y) {
					if (i->first < min_y) min_y = i->first;
					if (i->second > max_y) max_y = i->second;
					auto end_i = std::upper_bound(i, y.end(), max_y, [&](int a, auto& b) {
						return a < b.first;
					});
					if (std::prev(end_i)->second > max_y) max_y = std::prev(end_i)->second;
					y.erase(i, end_i);
				}
				y.insert(i, {min_y, max_y});
			}
		};

		auto add_neighbors = [&](int dir) {
			int vx = cardinal_direction_xy[dir].x;
			int vy = cardinal_direction_xy[dir].y;
			int v = dir & 1 ? w.cur_pos.x : w.cur_pos.y;
			int v0 = dir & 1 ? w.cur_pos.y : w.cur_pos.x;
			int dir_inner = w.inner[dir];
			int dir_outer = w.outer[dir];

			int v_min = dir & 1 ? w.cur_pos_min.x : w.cur_pos_min.y;
			int v_max = dir & 1 ? w.cur_pos_max.x : w.cur_pos_max.y;

			if (v + dir_inner == v_min || v + dir_inner == v_max) {
				int flags = dir & 1 ? 0x5c : 0xa6;
				if (dir & 2) flags = dir & 1 ? 0x53 : 0xa9;
				pf_add_neighbor(w.cur_pos + cardinal_direction_xy[dir] * (v == v_min ? v_max - v : v_min - v), flags);
				return true;
			}
			return false;
		};

		auto pf_generate_neighbors = [&](xy pos, int flags) {
			w.has_found_goal = false;
			w.cur_pos = pos;
			w.cur_pos_max = pos + xy(64, 64);
			w.cur_pos_min = pos - xy(64, 64);
			w.neighbors.clear();
			w.visited_areas.clear();

			if (pos.x < 64) w.cur_pos_min.x = 0;
			if (pos.y < 64) w.cur_pos_min.y = 0;
			if ((size_t)pos.x + 64 >= game_st.map_width) w.cur_pos_max.x = (int)game_st.map_width - 1;
			if ((size_t)pos.y + 64 >= game_st.map_height) w.cur_pos_max.y = (int)game_st.map_height - 1;
			if ((flags & (0x10 | (8|1))) == 0) w.cur_pos_min.y = pos.y;
			if ((flags & (0x20 | (1|2))) == 0) w.cur_pos_max.x = pos.x;
			if ((flags & (0x40 | (2|4))) == 0) w.cur_pos_max.y = pos.y;
			if ((flags & (0x80 | (4|8))) == 0) w.cur_pos_min.x = pos.x;

			for (auto& v : w.local_edges) v.clear();

			pf_add_local_terrain();
			pf_add_local_units();

			w.nearest_edge[0] = pf_local_edges_find(0, pos.y, pos.x, pos.x, w.cur_pos_min.y - 1);
			if (w.nearest_edge[0]) w.cur_pos_min.y = w.nearest_edge[0]->v[0] + w.inner[0] + 1;
			w.nearest_edge[1] = pf_local_edges_find(1, pos.x, pos.y, pos.y, w.cur_pos_max.x + 1);
			if (w.nearest_edge[1]) w.cur_pos_max.x = w.nearest_edge[1]->v[0] + w.inner[1] - 1;
			w.nearest_edge[2] = pf_local_edges_find(2, pos.y, pos.x, pos.x, w.cur_pos_max.y + 1);
			if (w.nearest_edge[2]) w.cur_pos_max.y = w.nearest_edge[2]->v[0] + w.inner[2] - 1;
			w.nearest_edge[3] = pf_local_edges_find(3, pos.x, pos.y, pos.y, w.cur_pos_min.x - 1);
			if (w.nearest_edge[3]) w.cur_pos_min.x = w.nearest_edge[3]->v[0] + w.inner[3] + 1;

			if (w.cur_pos_min.y == pos.y && w.cur_pos_max.x == pos.x) flags &= ~1;
			if (w.cur_pos_max.y == pos.y) {
				if (w.cur_pos_max.x == pos.x) flags &= ~2;
				if (w.cur_pos_min.x == pos.x) flags &= ~4;
			}
			if (w.cur_pos_min.y == pos.y) {
				if (w.cur_pos_min.x == pos.x) flags &= ~8;
				if (w.cur_pos_min.y == pos.y) flags &= ~0x10;
			}
			if (w.cur_pos_max.x == pos.x) flags &= ~0x20;
			if (w.cur_pos_max.y == pos.y) flags &= ~0x40;
			if (w.cur_pos_min.x == pos.x) flags &= ~0x80;

			if (flags & 0x10) {
				flags |= 9;
				pf_add_neighbor({pos.x, w.cur_pos_min.y}, w.nearest_edge[0] ? 0xa6 : 0xbf);
			}
			if (flags & 0x20) {
				flags |= 3;
				pf_add_neighbor({w.cur_pos_max.x, pos.y}, w.nearest_edge[1] ? 0x5c : 0x7f);
			}
			if (flags & 0x40) {
				flags |= 6;
				pf_add_neighbor({pos.x, w.cur_pos_max.y}, w.nearest_edge[2] ? 0xa9 : 0xef);
			}
			if (flags & 0x80) {
				flags |= 0xc;
				pf_add_neighbor({w.cur_pos_min.x, pos.y}, w.nearest_edge[3] ? 0x53 : 0xdf);
			}

			for (int dir = 0; dir != 4; ++dir) {
				if (flags & (1 << dir)) {
					if (add_neighbors(dir)) break;
				}
			}

			for (auto& v : w.visited_areas) {
				pf_mark_visited(v);
			}

		};

		bool has_found_goal = false;
		bool is_tired = false;
		int n_open_nodes_in_neighbor_region = 0;
		int n_open_nodes_in_target_region = 0;

		node_t* goal_node = nullptr;
		while (!open.empty()) {

			node_t* cur = open.front();
			std::swap(open.front(), open.back());
			open.pop_back();
			binary_heap_down(open.begin(), open.begin(), open.end(), cmp_node());
			if (cur->is_goal) {
				goal_node = cur;
				pf.destination_reached = true;
				break;
			}

			if (cur->directional_flags != 0) {
				cur->directional_flags = pf_remove_visited_flags(cur->pos, cur->directional_flags);
			}

			bool is_exhausted;
			if (n_open_nodes_in_target_region <= 0 || has_found_goal || (target_is_destination && all_nodes.size() < 150)) {
				is_exhausted = is_tired;
			} else {
				is_exhausted = true;
				is_tired = true;
			}
			if (n_open_nodes_in_neighbor_region && !has_found_goal) {
				if (all_nodes.size() >= 200) {
					is_exhausted = true;
					is_tired = true;
				}
			}
			if (cur->is_target_region && is_exhausted) {
				goal_node = cur;
				break;
			}
			cur->visited = true;
			if (cur->is_target_region) --n_open_nodes_in_target_region;
			if (cur->is_neighbor_region) {
				--n_open_nodes_in_neighbor_region;
				if (is_exhausted) break;
			}
			if (has_found_goal || is_exhausted || cur->directional_flags == 0) {
				continue;
			}
			pf_generate_neighbors(cur->pos, cur->directional_flags);

			bool found_goal = false;
			for (auto& v : w.neighbors) {
				if (v.is_goal) {
					found_goal = true;
				} else {
					if (v.flags) {
						v.flags = pf_remove_visited_flags(v.pos, v.flags);
						if (!v.flags) continue;
					}
				}
				auto* n_region = get_region_at(v.pos);
				fp8 cost = xy_length(to_xy_fp8(v.pos) - to_xy_fp8(cur->pos));
				if (target_region_walkable && n_region != target_region && n_region != source_region) {
					cost *= 2;
				}
				fp8 total_cost = cur->total_cost + cost;
				node_t* n = nullptr;
				for (auto i = std::next(all_nodes.begin()); i != all_nodes.end(); ++i) {
					if (i->pos == v.pos) {
						n = &*i;
						break;
					}
				}
				if (!n) {
					all_nodes.emplace_back();
					n = &all_nodes.back();
					n->prev = cur;
					n->pos = v.pos;
					n->region = n_region;
					n->depth = cur->depth + 1;
					n->directional_flags = v.flags;
					n->total_cost = total_cost;
					n->estimated_remaining_cost = xy_length(to_xy_fp8(target) - to_xy_fp8(n->pos));
					n->estimated_final_cost = n->total_cost + n->estimated_remaining_cost;
					n->visited = n->directional_flags == 0 && !n->is_goal;
					n->is_target_region = n->region == target_region;
					n->is_neighbor_region = n->region->pathfinder_flag != 0;
					n->is_goal = v.is_goal;
					if (!n->visited) {
						open.push_back(n);
						binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());
						if (n->is_target_region) ++n_open_nodes_in_target_region;
						if (n->is_neighbor_region) ++n_open_nodes_in_neighbor_region;
					}
					if (open.size() == 150) break;
					if (all_nodes.size() == 250) break;
				} else if (cur->prev != n) {
					if (total_cost < n->total_cost) {
						n->prev = cur;
						n->depth = cur->depth + 1;
						n->total_cost = total_cost;
						fp8 estimated_final_cost = n->total_cost + n->estimated_remaining_cost;
						if (n->visited) {
							n->directional_flags = v.flags;
							n->estimated_final_cost = estimated_final_cost;
							n->visited = false;
							n->is_goal = v.is_goal;
							open.push_back(n);
							binary_heap_up(std::prev(open.end()), open.begin(), open.end(), cmp_node());
							if (n->is_target_region) ++n_open_nodes_in_target_region;
							if (n->is_neighbor_region) ++n_open_nodes_in_neighbor_region;
						} else {
							n->estimated_final_cost = estimated_final_cost;
							binary_heap_up(std::find(open.begin(), open.end(), n), open.begin(), open.end(), cmp_node());
						}
						if (open.size() == 150) break;
					} else if (!n->visited) n->directional_flags &= v.flags;
				}
			}

			if (open.size() > pf.short_highest_open_size) {
				pf.short_highest_open_size = open.size();
			}

			has_found_goal = found_goal;
			if (open.size() == 150 || all_nodes.size() == 250) {
				if (!found_goal && !n_open_nodes_in_target_region) break;
			}
		}

		if (!goal_node) {
			int n_unvisited_nodes = 0;
			int n_unvisited_destination_region_nodes = 0;

			for (auto i = std::next(all_nodes.begin()); i != all_nodes.end(); ++i) {
				if (i->region->pathfinder_flag) ++i->region->pathfinder_flag;
				if (!i->visited) {
					if (i->directional_flags) i->directional_flags = pf_remove_visited_flags(i->pos, i->directional_flags);
					if (i->directional_flags) {
						++n_unvisited_nodes;
						if (i->region == destination_region) ++n_unvisited_destination_region_nodes;
					} else {
						i->visited = true;
						if (i->is_target_region) --n_open_nodes_in_target_region;
					}
				}
			}

			if (target_is_destination) {
				n_unvisited_nodes = n_unvisited_destination_region_nodes;
				for (auto* nr : move_to_region->walkable_neighbors) {
					if (nr == source_region) continue;
					if (nr->pathfinder_flag < 2) {
						++n_unvisited_nodes;
						break;
					} else {
						n_unvisited_nodes -= nr->pathfinder_flag / 2;
						if (n_unvisited_nodes < 0) n_unvisited_nodes = 0;
					}
				}
			}
			goal_node = start_node;
			start_node->estimated_remaining_cost = xy_length(to_xy_fp8(target - start_node->pos));
			if (n_unvisited_nodes == 0 && n_open_nodes_in_target_region == 0) {
				fp8 best_cost = fp8::integer(128 * 128);
				if (n_open_nodes_in_target_region == 0) best_cost = start_node->estimated_remaining_cost;
				node_t* best_node = start_node;
				for (auto i = std::next(all_nodes.begin()); i != all_nodes.end(); ++i) {
					if (i->estimated_remaining_cost < best_cost) {
						if (target_is_destination || i->region == source_region || (n_open_nodes_in_target_region && i->region == destination_region)) {
							best_cost = i->estimated_remaining_cost;
							best_node = &*i;
						}
					}
				}
				goal_node = best_node;
				pf.destination_reached = true;
				pf.is_stuck = true;
			} else {
				fp8 best_cost = fp8::integer(128 * 128);
				node_t* best_node = start_node;
				for (auto i = std::next(all_nodes.begin()); i != all_nodes.end(); ++i) {
					fp8 cost = i->estimated_remaining_cost;
					if (i->region == destination_region || i->region == target_region) {
						cost += i->total_cost / 2;
					} else {
						if (i->region->pathfinder_flag) {
							cost = cost * 3 / 2;
						} else {
							if (i->region == source_region) cost *= 2;
							else cost *= 4;
						}
					}
					if (i->visited) cost *= 32;
					if (cost < best_cost) {
						best_cost = cost;
						best_node = &*i;
					}
				}
				goal_node = best_node;
			}
		}

		pf.short_all_nodes_size = all_nodes.size();

		pf.short_path.clear();
		pf.current_short_path_index = 0;
		for (auto* n = goal_node; n != start_node && pf.short_path.size() < 128; n = n->prev) {
			pf.short_path.push_front(n->pos);
		}
		if (pf.short_path.size() > 50) {
			pf.short_path[49] = pf.short_path.back();
			pf.short_path.resize(50);
		}
		for (auto* nr : move_to_region->walkable_neighbors) {
			if (nr == source_region) continue;
			nr->pathfinder_flag = 0;
		}
	}

	std::pair<bool, xy> pathfinder_adjust_target_pos(rect unit_inner_bb, xy target) const {
		for (size_t width = 1; width < 10; width += 2) {
			for (size_t dir = 0; dir != 4; ++dir) {
				for (size_t n = 0; n != width; ++n) {
					auto bb = translate_rect(unit_inner_bb, target);
					if (is_in_inner_map_bounds(bb)) {
						bool entirely_walkable = true;
						size_t from_x = bb.from.x / 32u;
						size_t from_y = bb.from.y / 32u;
						size_t to_x = bb.to.x / 32u;
						size_t to_y = bb.to.y / 32u;
						for (size_t y = from_y; entirely_walkable && y <= to_y; ++y) {
							for (size_t x = from_x; x <= to_x; ++x) {
								size_t index = game_st.regions.tile_region_index.at(256 * y + x);
								if (index < 0x2000) {
									auto* region = &game_st.regions.regions[index];
									if (!region->walkable()) {
										entirely_walkable = false;
										break;
									}
								}
							}
						}
						if (entirely_walkable) return {true, target};
					}
					target += cardinal_direction_xy[dir] * 8;
				}
				target -= cardinal_direction_xy[dir] * 8;
			}
			target -= xy(8, 8);
		}
		return {false, target};
	}

	xy pathfinder_adjust_destination(const regions_t::region* source_region, xy destination) const {
		xy target = nearest_pos_in_rect(destination, source_region->area) / 32;
		for (size_t width = 1; width < 16; width += 2) {
			for (size_t dir = 0; dir != 4; ++dir) {
				for (size_t n = 0; n != width; ++n) {
					if ((size_t)target.x < game_st.map_tile_width && (size_t)target.y < game_st.map_tile_height) {
						const regions_t::region* r = get_region_at_prefer_walkable(target * 32u);
						if (r == source_region) return target * 32 + xy(16, 16);
					}
					target += cardinal_direction_xy[dir];
				}
				target -= cardinal_direction_xy[dir];
			}
			target -= xy(1, 1);
		}
		return to_xy(source_region->center);
	}

	bool pathfinder_find_next_short_path(pathfinder& pf) const {
		++pf.current_long_path_index;
		if (pf.current_long_path_index >= pf.long_path.size()) return false;
		const regions_t::region* source_region = pf.long_path[pf.current_long_path_index];
		while (source_region != pf.source_region) {
			++pf.current_long_path_index;
			if (pf.current_long_path_index >= pf.long_path.size()) return false;
			source_region = pf.long_path[pf.current_long_path_index];
		}
		xy target = pf.destination;
		bool is_near_destination = true;
		if (pf.long_path.size() - pf.current_long_path_index > 3) {
			target = to_xy(pf.long_path[pf.current_long_path_index + 2]->center);
			is_near_destination = false;
		}
		const regions_t::region* target_region = nullptr;
		if (pf.current_long_path_index != pf.long_path.size() - 1) {
			target_region = pf.long_path[pf.current_long_path_index + 1];
		} else {
			if (source_region != pf.destination_region) {
				pf.destination = pathfinder_adjust_destination(source_region, pf.destination);
				pf.is_stuck = true;
				is_near_destination = true;
				target_region = nullptr;
			}
		}
		if (is_near_destination) {
			pf.target_unit = pf.u->move_target.unit;
			if (!pf.target_unit) {
				auto unit_bb = unit_bounding_box(pf.u, pf.destination);
				auto add = xy(game_st.max_unit_width / 2 + 1, game_st.max_unit_height / 2 + 1);
				rect search_bb = {pf.destination - add, pf.destination + add};
				for (unit_t* u : find_units_noexpand(search_bb)) {
					if (is_intersecting(unit_bb, unit_sprite_bounding_box(u))) {
						if (pathfinder_unit_can_collide_with(pf, u)) {
							pf.target_unit = u;
							break;
						}
					}
				}
			}
			if (pf.target_unit) {
				if (unit_is_special_beacon(pf.target_unit)) {
					pf.target_unit = nullptr;
				}
			}
			if (pf.target_unit) {
				pf.target_unit_bb = unit_sprite_inner_bounding_box(pf.target_unit);
				pf.target_unit_bb.from -= pf.u->unit_type->dimensions.to + xy(1, 1);
				pf.target_unit_bb.to += pf.u->unit_type->dimensions.from + xy(1, 1);
				bool is_goal = false;
				if (pf.source.x == pf.target_unit_bb.from.x || pf.source.x == pf.target_unit_bb.to.x) {
					if (pf.source.y >= pf.target_unit_bb.from.y && pf.source.y <= pf.target_unit_bb.to.y) {
						is_goal = true;
					}
				}
				if (pf.source.y == pf.target_unit_bb.from.y || pf.source.y == pf.target_unit_bb.to.y) {
					if (pf.source.x >= pf.target_unit_bb.from.x && pf.source.x <= pf.target_unit_bb.to.x) {
						is_goal = true;
					}
				}
				if (is_goal) {
					pf.short_path = { pf.source };
					pf.destination = pf.source;
					pf.current_short_path_index = 0;
					pf.destination_reached = true;
					pf.is_stuck = false;
					return true;
				}
			} else {
				pf.target_unit_bb = {{-32000, -32000}, {-32000, -32000}};
				auto adjusted_target = pathfinder_adjust_target_pos(pf.unit_bb, target);
				if (adjusted_target.first && adjusted_target.second != target) {
					target = adjusted_target.second;
					pf.destination = target;
				}
			}

		}
		pathfinder_find_short_path(pf, target, target_region);
		if (!pf.short_path.empty()) {
			xy last_path_pos = pf.short_path.back();
			if (pf.destination_reached) {
				if (is_near_destination || last_path_pos != target) pf.destination = last_path_pos;
				else pf.destination_reached = false;
			} else {
				if (!target_region) {
					pf.destination = last_path_pos;
					pf.destination_reached = true;
					pf.is_stuck = true;
				}
			}
			return true;
		} else return false;
	}

	bool pathfinder_find_long_path(pathfinder& pf, xy from, xy to) const {
		pf.source = from;
		pf.destination = to;
		pf.source_region = get_region_at(pf.source);
		pf.destination_region = get_region_at(pf.destination);
		pf.long_all_nodes_size = 0;
		pf.long_highest_open_size = 0;
		pf.destination_reached = false;
		pf.is_stuck = false;
		if (pf.source_region == pf.destination_region) {
			pf.long_path = { pf.source_region };
			pf.full_long_path_size = 1;
			pf.current_long_path_index = (size_t)0 - 1;
			return true;
		} else {
			return pathfinder_find_long_path(pf);
		}
	}

	bool pathfinder_find(pathfinder& pf, bool short_path_only = false) {
		pf.source_region = get_region_at(pf.source);
		pf.destination_region = get_region_at(pf.destination);
		pf.unit_bb = unit_type_inner_bounding_box(pf.u->unit_type);
		pf.short_highest_open_size = 0;
		pf.short_all_nodes_size = 0;
		pf.long_all_nodes_size = 0;
		pf.long_highest_open_size = 0;
		pf.destination_reached = false;
		pf.is_stuck = false;
		if (short_path_only) return pathfinder_find_next_short_path(pf);
		if (pf.source_region == pf.destination_region) {
			pf.long_path = { pf.source_region };
			pf.full_long_path_size = 1;
			pf.current_long_path_index = (size_t)0 - 1;
			return pathfinder_find_next_short_path(pf);
		} else {
			if (!pathfinder_find_long_path(pf)) return false;
			return pathfinder_find_next_short_path(pf);
		}
	}


#endif // BWGAME_PATHFINDING_H
