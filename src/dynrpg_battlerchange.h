#ifndef EP_DYNBATTLERCHANGE_H
#define EP_DYNBATTLERCHANGE_H

#include "game_dynrpg.h"
#include <string>
#include <vector>

namespace DynRpg {

class BattlerChange : public DynRpgPlugin {
public:
	BattlerChange(Game_DynRpg& instance);
	~BattlerChange() override = default;

	bool Invoke(std::string_view func, dyn_arg_list args, bool& do_yield, Game_Interpreter* interpreter) override;
	void OnBattleStart() override;

	static void TransformEnemy(int enemy_idx, bool flash, bool adjust_mana);
	static void RepositionPartyMember(int party_slot);
	static void RepositionActor(int actor_id);
	static void RepositionEnemy(int enemy_idx);

	static void GetPartyMemberPosition(int party_slot);
	static void GetActorPosition(int actor_id);
	static void GetEnemyPosition(int enemy_idx);


private:
	void LoadConfig();

	int first_enemy_id_var = 0;
	int first_party_member_pos_var = 0;
	int first_actor_pos_var = 0;
	int first_enemy_pos_var = 0;
	int first_transform_flash_var = 0;

	bool transform_at_battle_start = true;
	bool flash_at_battle_start = false;
	bool flash_on_transform = true;
	bool adjust_mana_at_battle_start = true;
	bool adjust_mana_on_transform = true;
	bool reposition_party_members_at_battle_start = true;
	bool reposition_actors_at_battle_start = true;
	bool reposition_enemies_at_battle_start = true;
};

} // namespace DynRpg

#endif
