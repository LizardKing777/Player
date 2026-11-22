#ifndef EP_DYNRPG_ANIMATED_MONSTERS_H
#define EP_DYNRPG_ANIMATED_MONSTERS_H

#include "async_handler.h"
#include "game_dynrpg.h"
#include "memory_management.h"

#include <array>
#include <string>
#include <vector>

class Game_Enemy;

namespace DynRpg {

class AnimatedMonsters final : public DynRpgPlugin {
public:
	explicit AnimatedMonsters(Game_DynRpg& instance);
	~AnimatedMonsters() override;

	bool Invoke(std::string_view func, dyn_arg_list args, bool& do_yield, Game_Interpreter* interpreter) override;
	void Update() override;

	BitmapRef GetMonsterBitmap(const Game_Enemy& enemy) const;

private:
	static constexpr int num_pose_files = 6; // 1..3 idle, 4 dead, 5..6 hurt

	struct MonsterState {
		std::string name;
		int hue = 0;
		bool animated = false;
		int old_hp = 0;
		int hurt_timer = 0;
		int pose_index = 0;
		std::array<BitmapRef, num_pose_files> poses;
		std::array<FileRequestBinding, num_pose_files> requests;
	};

	void InitBattle();
	void InitMonster(MonsterState& state, const Game_Enemy& enemy, int index);
	void ClearBattle();
	void OnPoseReady(FileRequestResult* result, int monster_idx, int pose_idx, int hue);

	std::vector<MonsterState> monsters;
	int frame_timer = 0;
	bool in_battle = false;
};

} // namespace DynRpg

#endif
