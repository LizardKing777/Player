#include "dynrpg_battlerchange.h"
#include "filefinder.h"
#include "game_actors.h"
#include "game_actor.h"
#include "game_battle.h"
#include "game_enemy.h"
#include "game_enemyparty.h"
#include "game_map.h"
#include "game_party.h"
#include "game_variables.h"
#include "main_data.h"
#include "output.h"
#include "player.h"
#include "sprite_battler.h"
#include <lcf/inireader.h>
#include <lcf/data.h>
#include <lcf/reader_util.h>

namespace {
	DynRpg::BattlerChange* instance_ptr = nullptr;

	enum FlashIndex {
		FLASH_RED_INDEX = 0,
		FLASH_GREEN_INDEX = 1,
		FLASH_BLUE_INDEX = 2,
		FLASH_INTENSITY_INDEX = 3,
		FLASH_TIMER_INDEX = 4
	};
}

namespace DynRpg {

BattlerChange::BattlerChange(Game_DynRpg& instance)
	: DynRpgPlugin("DynBattlerChange", instance) {
	instance_ptr = this;
	LoadConfig();
}

void BattlerChange::LoadConfig() {
	auto is = FileFinder::Game().OpenFile("DynRPG.ini");
	if (!is) {
		Output::Warning("DynBattlerChange: Could not find DynRPG.ini");
		return;
	}

	lcf::INIReader ini(is);
	if (ini.ParseError() < 0) {
		Output::Warning("DynBattlerChange: Failed to read DynRPG.ini");
		return;
	}

	const std::string sec = "DynBattlerChange";
	first_enemy_id_var = ini.GetInteger(sec, "FirstEnemyIdVar", 0);
	first_party_member_pos_var = ini.GetInteger(sec, "FirstPartyMemberPosVar", 0);
	first_actor_pos_var = ini.GetInteger(sec, "FirstActorPosVar", 0);
	first_enemy_pos_var = ini.GetInteger(sec, "FirstEnemyPosVar", 0);
	first_transform_flash_var = ini.GetInteger(sec, "FirstTransformFlashVar", 0);

	transform_at_battle_start = ini.GetBoolean(sec, "TransformAtBattleStart", true);
	flash_at_battle_start = ini.GetBoolean(sec, "FlashAtBattleStart", false);
	flash_on_transform = ini.GetBoolean(sec, "FlashOnTransform", true);
	adjust_mana_at_battle_start = ini.GetBoolean(sec, "AdjustManaAtBattleStart", true);
	adjust_mana_on_transform = ini.GetBoolean(sec, "AdjustManaOnTransform", true);
	reposition_party_members_at_battle_start = ini.GetBoolean(sec, "RepositionPartyMembersAtBattleStart", true);
	reposition_actors_at_battle_start = ini.GetBoolean(sec, "RepositionActorsAtBattleStart", true);

	if (ini.HasValue(sec, "RepositionEnemiesAtBattleStart")) {
		reposition_enemies_at_battle_start = ini.GetBoolean(sec, "RepositionEnemiesAtBattleStart", true);
	} else if (ini.HasValue(sec, "RepositionAtBattleStart")) {
		reposition_enemies_at_battle_start = ini.GetBoolean(sec, "RepositionAtBattleStart", true);
	}

	Output::Debug("DynBattlerChange: Config loaded (EnemyIdVar={}, EnemyPosVar={}, PartyPosVar={})",
		first_enemy_id_var, first_enemy_pos_var, first_party_member_pos_var);
}

void BattlerChange::OnBattleStart() {
	Output::Debug("DynBattlerChange: OnBattleStart triggered");

	// Note: If you are testing through editor "Test Battle" mode, variables default to 0.
	if (Game_Battle::battle_test.enabled) {
		Output::Debug("DynBattlerChange: Skipping Battle Test mode");
		return;
	}

	if (transform_at_battle_start && first_enemy_id_var > 0) {
		int count = Main_Data::game_enemyparty->GetBattlerCount();
		for (int i = 0; i < count; ++i) {
			TransformEnemy(i, flash_at_battle_start, adjust_mana_at_battle_start);
		}
	}

	if (reposition_party_members_at_battle_start && first_party_member_pos_var > 0) {
		for (int i = 0; i < 4; ++i) {
			if (Main_Data::game_party->GetActor(i)) {
				RepositionPartyMember(i);
			}
		}
	} else if (reposition_actors_at_battle_start && first_actor_pos_var > 0) {
		for (int i = 0; i < 4; ++i) {
			auto* actor = Main_Data::game_party->GetActor(i);
			if (actor) {
				RepositionActor(actor->GetId());
			}
		}
	}

	if (reposition_enemies_at_battle_start && first_enemy_pos_var > 0) {
		int count = Main_Data::game_enemyparty->GetBattlerCount();
		for (int i = 0; i < count; ++i) {
			RepositionEnemy(i);
		}
	}
}

void BattlerChange::TransformEnemy(int enemy_idx, bool flash, bool adjust_mana) {
	if (!instance_ptr || instance_ptr->first_enemy_id_var <= 0) {
		return;
	}

	auto* enemy = Main_Data::game_enemyparty->GetEnemy(enemy_idx);
	if (!enemy) {
		return;
	}

	int var_id = instance_ptr->first_enemy_id_var + enemy_idx;
	int target_monster_id = Main_Data::game_variables->Get(var_id);

	if (target_monster_id > 0 && target_monster_id <= static_cast<int>(lcf::Data::enemies.size())) {
		int old_sp = enemy->GetSp();
		enemy->Transform(target_monster_id);

		if (flash) {
			int r = 31, g = 31, b = 31, intensity = 31, timer = 20;
			int flash_var = instance_ptr->first_transform_flash_var;
			if (flash_var > 0) {
				int v_r = Main_Data::game_variables->Get(flash_var + FLASH_RED_INDEX);
				int v_g = Main_Data::game_variables->Get(flash_var + FLASH_GREEN_INDEX);
				int v_b = Main_Data::game_variables->Get(flash_var + FLASH_BLUE_INDEX);
				int v_i = Main_Data::game_variables->Get(flash_var + FLASH_INTENSITY_INDEX);
				int v_t = Main_Data::game_variables->Get(flash_var + FLASH_TIMER_INDEX);

				if (v_r >= 0) r = v_r;
				if (v_g >= 0) g = v_g;
				if (v_b >= 0) b = v_b;
				if (v_i >= 0) intensity = v_i;
				if (v_t >= 0) timer = v_t;
			}
			enemy->Flash(r, g, b, intensity, timer);
		}

		if (adjust_mana) {
			if (old_sp < enemy->GetSp()) {
				enemy->SetSp(old_sp);
			}
		}
	} else if (target_monster_id == 0) {
		enemy->SetHidden(true);
	}
}

void BattlerChange::RepositionPartyMember(int party_slot) {
	if (!instance_ptr || instance_ptr->first_party_member_pos_var <= 0) {
		return;
	}

	auto* actor = Main_Data::game_party->GetActor(party_slot);
	if (!actor) {
		return;
	}

	int base_var = instance_ptr->first_party_member_pos_var + (2 * party_slot);
	int var_x = Main_Data::game_variables->Get(base_var);
	int var_y = Main_Data::game_variables->Get(base_var + 1);

	Point new_pos = actor->GetOriginalPosition();
	if (var_x >= 0) {
		new_pos.x = var_x;
	}
	if (var_y >= 0) {
		new_pos.y = var_y;
	}

	actor->SetBattlePosition(new_pos);
	auto* sprite = actor->GetBattleSprite();
	if (sprite) {
		sprite->ResetZ();
	}
}

void BattlerChange::RepositionActor(int actor_id) {
	if (!instance_ptr || instance_ptr->first_actor_pos_var <= 0) {
		return;
	}

	auto* actor = Main_Data::game_actors->GetActor(actor_id);
	if (!actor || !actor->IsInParty()) {
		return;
	}

	int base_var = instance_ptr->first_actor_pos_var + 2 * (actor_id - 1);
	int var_x = Main_Data::game_variables->Get(base_var);
	int var_y = Main_Data::game_variables->Get(base_var + 1);

	Point new_pos = actor->GetOriginalPosition();
	if (var_x >= 0) {
		new_pos.x = var_x;
	}
	if (var_y >= 0) {
		new_pos.y = var_y;
	}

	actor->SetBattlePosition(new_pos);
	auto* sprite = actor->GetBattleSprite();
	if (sprite) {
		sprite->ResetZ();
	}
}

void BattlerChange::RepositionEnemy(int enemy_idx) {
	if (!instance_ptr || instance_ptr->first_enemy_pos_var <= 0) {
		return;
	}

	auto* enemy = Main_Data::game_enemyparty->GetEnemy(enemy_idx);
	if (!enemy) {
		return;
	}

	int base_var = instance_ptr->first_enemy_pos_var + (2 * enemy_idx);
	int var_x = Main_Data::game_variables->Get(base_var);
	int var_y = Main_Data::game_variables->Get(base_var + 1);

	Point new_pos = enemy->GetOriginalPosition();
	if (var_x >= 0) {
		new_pos.x = var_x;
	}
	if (var_y >= 0) {
		new_pos.y = var_y;
	}

	enemy->SetBattlePosition(new_pos);
	auto* sprite = enemy->GetBattleSprite();
	if (sprite) {
		sprite->ResetZ();
	}
}

void BattlerChange::GetPartyMemberPosition(int party_slot) {
	if (!instance_ptr || instance_ptr->first_party_member_pos_var <= 0) {
		return;
	}

	auto* actor = Main_Data::game_party->GetActor(party_slot);
	if (!actor) {
		return;
	}

	int base_var = instance_ptr->first_party_member_pos_var + (2 * party_slot);
	Point pos = Game_Battle::IsBattleRunning() ? actor->GetBattlePosition() : actor->GetOriginalPosition();

	Main_Data::game_variables->Set(base_var, pos.x);
	Main_Data::game_variables->Set(base_var + 1, pos.y);
	Game_Map::SetNeedRefresh(true);
}

void BattlerChange::GetActorPosition(int actor_id) {
	if (!instance_ptr || instance_ptr->first_actor_pos_var <= 0) {
		return;
	}

	auto* actor = Main_Data::game_actors->GetActor(actor_id);
	if (!actor) {
		return;
	}

	int base_var = instance_ptr->first_actor_pos_var + 2 * (actor_id - 1);
	Point pos = (Game_Battle::IsBattleRunning() && actor->IsInParty()) ? actor->GetBattlePosition() : actor->GetOriginalPosition();

	Main_Data::game_variables->Set(base_var, pos.x);
	Main_Data::game_variables->Set(base_var + 1, pos.y);
	Game_Map::SetNeedRefresh(true);
}

void BattlerChange::GetEnemyPosition(int enemy_idx) {
	if (!instance_ptr || instance_ptr->first_enemy_pos_var <= 0 || !Game_Battle::IsBattleRunning()) {
		return;
	}

	auto* enemy = Main_Data::game_enemyparty->GetEnemy(enemy_idx);
	if (!enemy) {
		return;
	}

	int base_var = instance_ptr->first_enemy_pos_var + (2 * enemy_idx);
	Point pos = enemy->GetBattlePosition();

	Main_Data::game_variables->Set(base_var, pos.x);
	Main_Data::game_variables->Set(base_var + 1, pos.y);
	Game_Map::SetNeedRefresh(true);
}

bool BattlerChange::Invoke(std::string_view func, dyn_arg_list args, bool&, Game_Interpreter*) {
	bool okay = false;

	if (func == "dynbattlerchange_transform_enemy") {
		int enemy_id = 0;
		std::tie(enemy_id) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && enemy_id >= 1) {
			TransformEnemy(enemy_id - 1, flash_on_transform, adjust_mana_on_transform);
		}
		return true;
	}
	if (func == "dynbattlerchange_reposition_party_member") {
		int slot = 0;
		std::tie(slot) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && slot >= 1) {
			RepositionPartyMember(slot - 1);
		}
		return true;
	}
	if (func == "dynbattlerchange_reposition_actor") {
		int actor_id = 0;
		std::tie(actor_id) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && actor_id >= 1) {
			RepositionActor(actor_id);
		}
		return true;
	}
	if (func == "dynbattlerchange_reposition_enemy") {
		int enemy_id = 0;
		std::tie(enemy_id) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && enemy_id >= 1) {
			RepositionEnemy(enemy_id - 1);
		}
		return true;
	}
	if (func == "dynbattlerchange_get_party_member_position") {
		int slot = 0;
		std::tie(slot) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && slot >= 1) {
			GetPartyMemberPosition(slot - 1);
		}
		return true;
	}
	if (func == "dynbattlerchange_get_actor_position") {
		int actor_id = 0;
		std::tie(actor_id) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && actor_id >= 1) {
			GetActorPosition(actor_id);
		}
		return true;
	}
	if (func == "dynbattlerchange_get_enemy_position") {
		int enemy_id = 0;
		std::tie(enemy_id) = DynRpg::ParseArgs<int>(func, args, &okay);
		if (okay && enemy_id >= 1) {
			GetEnemyPosition(enemy_id - 1);
		}
		return true;
	}

	return false;
}

} // namespace DynRpg
