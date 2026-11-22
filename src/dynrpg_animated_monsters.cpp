#include "dynrpg_animated_monsters.h"
#include "bitmap.h"
#include "cache.h"
#include "filefinder.h"
#include "game_battle.h"
#include "game_enemy.h"
#include "game_enemyparty.h"
#include "main_data.h"
#include "output.h"

#include <array>
#include <string>

namespace {
	// Idle: 1-2-3-2, Dead: 4, Hurt: 5-6 (0-based indices into MonsterState::poses)
	constexpr std::array<int, 7> animation_cycle = { 0, 1, 2, 1, 3, 4, 5 };
}

DynRpg::AnimatedMonsters::AnimatedMonsters(Game_DynRpg& instance) :
	DynRpgPlugin("AnimatedMonsters", instance) {
}

DynRpg::AnimatedMonsters::~AnimatedMonsters() {
	OnBattleEnd();
}

bool DynRpg::AnimatedMonsters::Invoke(std::string_view, dyn_arg_list, bool&, Game_Interpreter*) {
	return false;
}

void DynRpg::AnimatedMonsters::InitMonster(MonsterState& state, const Game_Enemy& enemy, int index) {
	state = {};
	state.name = ToString(enemy.GetName());
	state.hue = enemy.GetHue();
	state.old_hp = enemy.GetHp();
	state.old_blink_timer = enemy.GetBlinkTimer();
	state.idle_step = index % 4;
	state.pose_index = state.idle_step;

	if (state.name.empty()) {
		return;
	}

	std::string first_pose = FileFinder::MakePath(state.name, "1");
	if (FileFinder::FindImage("Monster", first_pose).empty()) {
		return;
	}

	state.animated = true;

	for (int p = 0; p < num_pose_files; ++p) {
		std::string pose_path = FileFinder::MakePath(state.name, std::to_string(p + 1));
		FileRequestAsync* request = AsyncHandler::RequestFile("Monster", pose_path);
		request->SetGraphicFile(true);
		state.requests[p] = request->Bind(&AnimatedMonsters::OnPoseReady, this, index, p, state.hue);
		request->Start();
	}
}

void DynRpg::AnimatedMonsters::OnPoseReady(FileRequestResult* result, int monster_idx, int pose_idx, int hue) {
	if (monster_idx < 0 || monster_idx >= static_cast<int>(monsters.size())) {
		return;
	}

	auto& state = monsters[monster_idx];
	if (!state.animated) {
		return;
	}

	if (FileFinder::FindImage("Monster", result->file).empty()) {
		return;
	}

	BitmapRef graphic = Cache::Monster(result->file);
	if (hue != 0 && graphic) {
		BitmapRef hue_graphic = Bitmap::Create(graphic->GetWidth(), graphic->GetHeight());
		hue_graphic->HueChangeBlit(0, 0, *graphic, graphic->GetRect(), hue);
		hue_graphic->SetId(fmt::format("{},hue={}", graphic->GetId(), hue));
		graphic = hue_graphic;
	}

	state.poses[pose_idx] = std::move(graphic);
}

void DynRpg::AnimatedMonsters::OnBattleStart() {
	OnBattleEnd();

	const auto& enemies = Main_Data::game_enemyparty->GetEnemies();
	monsters.resize(enemies.size());

	for (size_t i = 0; i < enemies.size(); ++i) {
		InitMonster(monsters[i], *enemies[i], static_cast<int>(i));
	}

	frame_timer = 0;
	in_battle = true;
}

void DynRpg::AnimatedMonsters::OnBattleEnd() {
	monsters.clear();
	frame_timer = 0;
	in_battle = false;
}

void DynRpg::AnimatedMonsters::Update() {
	if (!Game_Battle::IsBattleRunning()) {
		if (in_battle) {
			OnBattleEnd();
		}
		return;
	}

	if (!in_battle) {
		OnBattleStart();
		return;
	}

	const auto& enemies = Main_Data::game_enemyparty->GetEnemies();
	if (monsters.size() != enemies.size()) {
		OnBattleStart();
		return;
	}

	++frame_timer;
	if (frame_timer > 10) {
		frame_timer = 0;
		for (auto& state : monsters) {
			state.idle_step = (state.idle_step + 1) % 4;
		}
	}

	for (size_t i = 0; i < enemies.size(); ++i) {
		auto* enemy = enemies[i];
		auto& state = monsters[i];

		// Handle Transform command changing the enemy database entry mid-battle
		if (state.name != enemy->GetName() || state.hue != enemy->GetHue()) {
			InitMonster(state, *enemy, static_cast<int>(i));
			continue;
		}

		if (!state.animated) {
			continue;
		}

		// RM2k mode calls SetBlinkTimer() during the damage message before ApplyHpEffect()
		// reduces HP. Trigger hurt_timer immediately on a new blink so it syncs with the hit.
		int bt = enemy->GetBlinkTimer();
		if (bt > state.old_blink_timer) {
			state.hurt_timer = 30;
			state.hurt_triggered_by_blink = true;
		} else if (bt == 0) {
			state.hurt_triggered_by_blink = false;
		}
		state.old_blink_timer = bt;

		int hp = enemy->GetHp();
		if (hp < state.old_hp) {
			if (!state.hurt_triggered_by_blink) {
				state.hurt_timer = 30;
			}
			state.hurt_triggered_by_blink = false;
			state.old_hp = hp;
		} else if (hp > state.old_hp) {
			state.old_hp = hp;
		}

		if (hp <= 0) {
			state.pose_index = 4;
		} else if (state.hurt_timer > 0) {
			state.pose_index = 5 + ((state.hurt_timer - 1) / 5) % 2;
			--state.hurt_timer;
		} else {
			state.pose_index = state.idle_step;
		}
	}
}

BitmapRef DynRpg::AnimatedMonsters::GetMonsterBitmap(const Game_Enemy& enemy) const {
	if (!in_battle) {
		return nullptr;
	}

	int idx = Main_Data::game_enemyparty->GetEnemyPositionInParty(&enemy);
	if (idx < 0 || idx >= static_cast<int>(monsters.size())) {
		return nullptr;
	}

	const auto& state = monsters[idx];
	if (!state.animated) {
		return nullptr;
	}

	int file_idx = animation_cycle[state.pose_index];
	if (state.poses[file_idx]) {
		return state.poses[file_idx];
	}

	return state.poses[0];
}
