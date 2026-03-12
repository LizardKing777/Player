/*
 * This file is part of EasyRPG Player.
 * ... (license header) ...
 */

// Headers
#include <map>
#include <memory>
#include <lcf/reader_util.h>
#include <algorithm>

#include "dynrpg_textplugin.h"
#include "baseui.h"
#include "bitmap.h"
#include "drawable.h"
#include "drawable_mgr.h"
#include "game_map.h"
#include "game_message.h"
#include "game_pictures.h"
#include "game_variables.h"
#include "main_data.h"
#include "pending_message.h"
#include "text.h"
#include "cache.h"
#include "utils.h"

// --- MODIFICATION ---
// Include the game_map header for Mode7 functions and Cache for textures
#include "game_map.h"
#include "cache.h"
// --- END MODIFICATION ---

class DynRpgText;

namespace {
	std::map<std::string, std::unique_ptr<DynRpgText>> graphics;
}

class DynRpgText : public Drawable {
public:
	// --- MODIFICATION ---
	// Enum to define how the text should be rendered in Mode7
	enum class RenderPlane {
		Screen, // Normal 2D overlay
		Map,    // Flat on the map ground
		Sprite  // Upright like an event sprite
	};
	// --- END MODIFICATION ---

	DynRpgText(int pic_id, int x, int y, const std::string& text) : Drawable(0), pic_id(pic_id), x(x), y(y) {
		DrawableMgr::Register(this);
		AddLine(text);
	}

	DynRpgText(int pic_id, int x, int y, const std::vector<std::string>& text) : Drawable(0), pic_id(pic_id), x(x), y(y) {
		DrawableMgr::Register(this);
		for (auto& s : text) {
			AddLine(s);
		}
	}

	void AddLine(const std::string& text) {
		texts.push_back(text);
		Refresh();
	}

	void AddText(const std::string& text) {
		if (texts.empty()) {
			texts.push_back(text);
		} else {
			texts.back() += text;
		}
		Refresh();
	}

	void ClearText() {
		texts.clear();
		Refresh();
	}

	void SetPosition(int new_x, int new_y) {
		x = new_x;
		y = new_y;
	}

	void SetColor(int new_color) {
		color = new_color;
		Refresh();
	}

	void SetPictureId(int new_pic_id) {
		pic_id = new_pic_id;
		const Game_Pictures::Picture* pic = Main_Data::game_pictures->GetPicturePtr(pic_id);
		if (!pic) return;
		const Sprite_Picture* sprite = pic->sprite.get();
		if (!sprite) return;
		SetZ(sprite->GetZ() + 1);
	}

	void SetFixed(bool fixed) {
		this->fixed = fixed;
	}

	// --- MODIFICATION ---
	// New setters for Mode7 properties
	void SetRenderPlane(std::string_view plane) {
		std::string p = Utils::LowerCase(plane);
		if (p == "map") {
			render_plane = RenderPlane::Map;
		} else if (p == "sprite") {
			render_plane = RenderPlane::Sprite;
		} else {
			render_plane = RenderPlane::Screen;
		}
	}

	void SetZOffset(int offset) {
		z_offset = offset;
	}

	void SetTexture(std::string_view filename) {
		texture_name = ToString(filename);
		if (texture_name.empty()) {
			texture_bitmap.reset();
		} else {
			texture_bitmap = Cache::Picture(texture_name, true);
		}
		Refresh();
	}
	// --- END MODIFICATION ---

	void Draw(Bitmap& dst) override {
		if (!bitmap) return;

		const Game_Pictures::Picture* pic = Main_Data::game_pictures->GetPicturePtr(pic_id);
		if (!pic) return;
		const Sprite_Picture* sprite = pic->sprite.get();
		if (!sprite) return;

		// --- MODIFICATION ---
		// Mode7 rendering logic
		if (Game_Map::GetIsMode7() && fixed && render_plane != RenderPlane::Screen) {
			int map_display_x = Game_Map::GetDisplayX() / 16;
			int map_display_y = Game_Map::GetDisplayY() / 16;

			switch (render_plane) {
				case RenderPlane::Sprite: {
					// Stands upright on the map (billboard)
					int screen_x = x - map_display_x;
					int screen_y = y - map_display_y;

					auto transform = Game_Map::TransformToMode7(screen_x, screen_y);

					if (transform.zoom <= 0) return; // Behind horizon

					float scaled_width = bitmap->GetWidth() * transform.zoom;
					float scaled_height = bitmap->GetHeight() * transform.zoom;

					float draw_x = transform.screen_x - scaled_width / 2.0f;
					float draw_y = transform.screen_y - scaled_height - (z_offset * transform.zoom);

					Rect dst_rect(draw_x, draw_y, scaled_width, scaled_height);
					dst.StretchBlit(dst_rect, *bitmap, bitmap->GetRect(), sprite->GetOpacity());
					break;
				}
				case RenderPlane::Map: {
					// Lies flat on the map (perspective scanline rendering)
					const int text_height = bitmap->GetHeight();
					if (text_height <= 0) return;

					// Transform the top and bottom points of the text sprite
					auto transform_bottom = Game_Map::TransformToMode7(x - map_display_x, y - z_offset - map_display_y);
					auto transform_top = Game_Map::TransformToMode7(x - map_display_x, y - text_height - z_offset - map_display_y);

					// If the whole thing is behind the horizon, don't draw
					if (transform_bottom.zoom <= 0 && transform_top.zoom <= 0) {
						return;
					}

					// Loop through each scanline of the source bitmap
					for (int y_scan = 0; y_scan < text_height; ++y_scan) {
						float t = static_cast<float>(y_scan) / (text_height - 1);

						// Interpolate screen y position and zoom factor for the current scanline
						float current_sy = transform_top.screen_y + t * (transform_bottom.screen_y - transform_top.screen_y);
						float current_zoom = transform_top.zoom + t * (transform_bottom.zoom - transform_top.zoom);

						// Skip scanlines that are behind the horizon
						if (current_zoom <= 0) continue;

						// Calculate the screen width and X position for this scanline
						float scaled_width = bitmap->GetWidth() * current_zoom;
						float draw_x = transform_bottom.screen_x - scaled_width / 2.0f; // Use bottom's X for consistent centering

						// Determine the height of this scanline on the screen
						float next_sy;
						if (y_scan < text_height - 1) {
							float next_t = static_cast<float>(y_scan + 1) / (text_height - 1);
							next_sy = transform_top.screen_y + next_t * (transform_bottom.screen_y - transform_top.screen_y);
						} else {
							next_sy = current_sy + 1.0f; // Last line is at least 1 pixel high
						}
						float line_height = std::max(1.0f, next_sy - current_sy);

						Rect src_rect(0, y_scan, bitmap->GetWidth(), 1);
						Rect dst_rect(draw_x, current_sy, scaled_width, line_height);

						dst.StretchBlit(dst_rect, *bitmap, src_rect, sprite->GetOpacity());
					}
					break;
				}
				default:
					goto draw_2d;
			}
		} else {
		draw_2d:
			// Original 2D drawing logic with z_offset
			if (fixed) {
				dst.Blit(x - Game_Map::GetDisplayX() / 16, y - Game_Map::GetDisplayY() / 16 + 2 - z_offset, *bitmap, bitmap->GetRect(), sprite->GetOpacity());
			} else {
				dst.Blit(x, y + 2, *bitmap, bitmap->GetRect(), sprite->GetOpacity());
			}
		}
		// --- END MODIFICATION ---
	};

	void Update() {
		if (GetZ() == 0) {
			SetPictureId(pic_id);
		}
	}

	std::vector<uint8_t> Save(const std::string& id) {
		std::stringstream ss;
		ss << x << "," << y << ",";
		for (int i = 0; i < static_cast<int>(texts.size()); ++i) {
			std::string t = texts[i];
			std::replace(t.begin(), t.end(), ',', '\1');
			ss << t;
			if (i < static_cast<int>(texts.size()) - 1) {
				ss << "\n";
			}
		}
		ss << "," << color << "," << id;
		ss << "," << 255 << "," << (fixed ? "1" : "0") << "," << pic_id;
		std::vector<uint8_t> data;
		std::string s = ss.str();
		data.assign(s.begin(), s.end());
		return data;
	}

	static DynRpgText* GetTextHandle(const std::string& id, bool silent = false) {
		PendingMessage pm(CommandCodeInserter);
		pm.PushLine(id);
		std::string new_id = pm.GetLines().front();
		auto it = graphics.find(new_id);
		if (it == graphics.end()) {
			if (!silent) {
				Output::Warning("No text with ID %s found", new_id.c_str());
			}
			return nullptr;
		}
		return it->second.get();
	}

	static std::optional<std::string> CommandCodeInserter(char ch, const char** iter, const char* end, uint32_t escape_char) {
		if (ch == 'I' || ch == 'i') {
			auto parse_ret = Game_Message::ParseParam('I', 'i', *iter, end, escape_char, true);
			*iter = parse_ret.next;
			int value = parse_ret.value;
			const auto* item = lcf::ReaderUtil::GetElement(lcf::Data::items, value);
			if (!item) {
				Output::Warning("Invalid Item Id {} in DynTextPlugin text", value);
				return "";
			} else{
				return ToString(ch == 'i' ? item->name : item->description);
			}
		} else if (ch == 'T' || ch == 't') {
			auto parse_ret = Game_Message::ParseParam('T', 't', *iter, end, escape_char, true);
			*iter = parse_ret.next;
			int value = parse_ret.value;
			const auto* skill = lcf::ReaderUtil::GetElement(lcf::Data::skills, value);
			if (!skill) {
				Output::Warning("Invalid Item Id {} in DynTextPlugin text", value);
				return "";
			} else{
				return ToString(ch == 't' ? skill->name : skill->description);
			}
		} else if (ch == 'x' || ch == 'X') {
			auto parse_ret = Game_Message::ParseStringParam('X', 'x', *iter, end, escape_char, true);
			*iter = parse_ret.next;
			std::string value = parse_ret.value;
			auto* handle = GetTextHandle(value);
			if (handle) {
				return handle->texts[0];
			} else {
				return "";
			}
		}
		return PendingMessage::DefaultCommandInserter(ch, iter, end, escape_char);
	}

private:
	void Refresh() {
		if (texts.empty()) {
			bitmap.reset();
			return;
		}

		int width = 0;
		int height = 0;
		const FontRef& font = Font::Default();

		for (auto& t : texts) {
			PendingMessage pm(CommandCodeInserter);
			pm.PushLine(t);
			t = pm.GetLines().front();
			Rect r = Text::GetSize(*font, t);
			width = std::max(width, r.width);
			height += r.height + 2;
		}

		bitmap = Bitmap::Create(width, height, true);

		// --- MODIFICATION ---
		// Draw texture background if it exists
		if (texture_bitmap) {
			bitmap->TiledBlit(Rect(0, 0, texture_bitmap->GetWidth(), texture_bitmap->GetHeight()), *texture_bitmap, bitmap->GetRect(), Opacity::Opaque());
		}
		// --- END MODIFICATION ---

		height = 0;
		for (auto& t : texts) {
			bitmap->TextDraw(0, height, color, t);
			height += Text::GetSize(*font, t).height + 2;
		}

		SetPictureId(pic_id);
	}

	std::vector<std::string> texts;
	BitmapRef bitmap;
	int pic_id = 1;
	int x = 0;
	int y = 0;
	int color = 0;
	bool fixed = false;

	// --- MODIFICATION ---
	// New members for Mode7 properties
	RenderPlane render_plane = RenderPlane::Screen;
	int z_offset = 0;
	BitmapRef texture_bitmap;
	std::string texture_name;
	// --- END MODIFICATION ---
};

static bool WriteText(dyn_arg_list args) {
	auto func = "write_text";
	bool okay;
	std::string id, text;
	int x, y;

	std::tie(id, x, y, text) = DynRpg::ParseArgs<std::string, int, int, std::string>(func, args, &okay);
	if (!okay) return true;

	PendingMessage pm(DynRpgText::CommandCodeInserter);
	pm.PushLine(id);
	std::string new_id = pm.GetLines().front();
	graphics[new_id] = std::make_unique<DynRpgText>(1, x, y, text);

	if (args.size() > 4) {
		std::string fixed = std::get<0>(DynRpg::ParseArgs<std::string>(func, args.subspan(4), &okay));
		if (!okay) return true;
		graphics[new_id]->SetFixed(fixed == "fixed");
	}

	if (args.size() > 5) {
		int color = std::get<0>(DynRpg::ParseArgs<int>(func, args.subspan(5), &okay));
		if (!okay) return true;
		graphics[new_id]->SetColor(color);
	}

	if (args.size() > 6) {
		int pic_id = std::get<0>(DynRpg::ParseArgs<int>(func, args.subspan(6), &okay));
		if (!okay) return true;
		graphics[new_id]->SetPictureId(pic_id);
	}

	return true;
}

static bool AppendLine(dyn_arg_list args) {
	auto func = "append_line";
	bool okay;
	std::string id, text;
	std::tie(id, text) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (!handle) return true;
	handle->AddLine(text);
	return true;
}

static bool AppendText(dyn_arg_list args) {
	auto func = "append_text";
	bool okay;
	std::string id, text;
	std::tie(id, text) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (!handle) return true;
	handle->AddText(text);
	return true;
}

static bool ChangeText(dyn_arg_list args) {
	auto func = "change_text";
	bool okay;
	std::string id, text, color;
	std::tie(id, text, color) = DynRpg::ParseArgs<std::string, std::string, std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (!handle) return true;
	handle->ClearText();
	if (color != "end") {
		handle->SetColor(atoi(color.c_str()));
	}
	handle->AddText(text);
	return true;
}

static bool ChangePosition(dyn_arg_list args) {
	auto func = "change_position";
	bool okay;
	std::string id;
	int x, y;
	std::tie(id, x, y) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (!handle) return true;
	handle->SetPosition(x, y);
	return true;
}

static bool RemoveText(dyn_arg_list args) {
	auto func = "remove_text";
	bool okay;
	std::string id;
	std::tie(id) = DynRpg::ParseArgs<std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id, true);
	if (!handle) return true;
	handle->ClearText();
	return true;
}

static bool RemoveAll(dyn_arg_list) {
	graphics.clear();
	return true;
}

// --- MODIFICATION ---
// New static functions to handle plugin commands
static bool SetRenderPlane(dyn_arg_list args) {
	auto func = "text_set_render_plane";
	bool okay;
	std::string id, plane;
	std::tie(id, plane) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (handle) {
		handle->SetRenderPlane(plane);
	}
	return true;
}

static bool SetZOffset(dyn_arg_list args) {
	auto func = "text_set_z_offset";
	bool okay;
	std::string id;
	int offset;
	std::tie(id, offset) = DynRpg::ParseArgs<std::string, int>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (handle) {
		handle->SetZOffset(offset);
	}
	return true;
}

static bool SetTexture(dyn_arg_list args) {
	auto func = "text_set_texture";
	bool okay;
	std::string id, filename;
	std::tie(id, filename) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) return true;
	DynRpgText* handle = DynRpgText::GetTextHandle(id);
	if (handle) {
		handle->SetTexture(filename);
	}
	return true;
}
// --- END MODIFICATION ---

bool DynRpg::TextPlugin::Invoke(std::string_view func, dyn_arg_list args, bool&, Game_Interpreter*) {
	if (func == "write_text") {
		return WriteText(args);
	} else if (func == "append_line") {
		return AppendLine(args);
	} else if (func == "append_text") {
		return AppendText(args);
	} else if (func == "change_text") {
		return ChangeText(args);
	} else if (func == "change_position") {
		return ChangePosition(args);
	} else if (func == "remove_text") {
		return RemoveText(args);
	} else if (func == "remove_all") {
		return RemoveAll(args);
	}
	// --- MODIFICATION ---
	// Add new commands to the chain
	else if (func == "text_set_render_plane") {
		return SetRenderPlane(args);
	} else if (func == "text_set_z_offset") {
		return SetZOffset(args);
	} else if (func == "text_set_texture") {
		return SetTexture(args);
	}
	// --- END MODIFICATION ---
	return false;
}

void DynRpg::TextPlugin::Update() {
	for (auto& g : graphics) {
		g.second->Update();
	}
}

DynRpg::TextPlugin::~TextPlugin() {
	graphics.clear();
}

void DynRpg::TextPlugin::Load(const std::vector<uint8_t>& in_buffer) {
	size_t counter = 0;
	std::string str(reinterpret_cast<const char*>(in_buffer.data()), in_buffer.size());
	std::vector<std::string> tokens = Utils::Tokenize(str, [](char32_t c) { return c == ','; });

	int x = 0, y = 0, color = 0, pic_id = 1;
	std::vector<std::string> texts;
	std::string id = "";
	bool fixed = false;

	for (auto& t : tokens) {
		switch (counter) {
			case 0: x = atoi(t.c_str()); break;
			case 1: y = atoi(t.c_str()); break;
			case 2: {
				std::replace(t.begin(), t.end(), '\1', ',');
				texts = Utils::Tokenize(t, [](char32_t c) { return c == '\n'; });
				break;
			}
			case 3: color = atoi(t.c_str()); break;
			case 4: /* ignore transparency */ break;
			case 5: fixed = t == "1"; break;
			case 6: pic_id = atoi(t.c_str()); break;
			case 7: id = t; break;
		}
		counter++;
		if (counter == 8) {
			counter = 0;
			graphics[id] = std::make_unique<DynRpgText>(pic_id, x, y, texts);
			texts.clear();
			graphics[id]->SetColor(color);
			graphics[id]->SetFixed(fixed);
		}
	}
}

std::vector<uint8_t> DynRpg::TextPlugin::Save() {
	std::vector<uint8_t> save_data;
	std::stringstream ss;
	bool first = true;
	for (auto& g : graphics) {
		if (!first) {
			ss << ',';
		}
		std::vector<uint8_t> res = g.second->Save(g.first);
		ss.write(reinterpret_cast<const char*>(res.data()), res.size());
		first = false;
	}
	std::string s = ss.str();
	save_data.assign(s.begin(), s.end());
	return save_data;
}
