#include "../../include/Particles/ParticleXml.h"
#include "../../include/PopState.h"

#include <pugixml.hpp>

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace
{
	using pop::particles::CurveType;
	using pop::particles::EmitterType;
	using pop::particles::FloatTrack;
	using pop::particles::FloatTrackNode;
	using pop::particles::ParticleEmitterDefinition;
	using pop::particles::ParticleFieldDefinition;
	using pop::particles::ParticleFieldType;
	using pop::particles::ParticleFlags;

	struct CurveName
	{
		CurveType value;
		const char* name;
	};

	struct EmitterTypeName
	{
		EmitterType value;
		const char* name;
	};

	struct FieldTypeName
	{
		ParticleFieldType value;
		const char* name;
	};

	struct ParticleFlagName
	{
		ParticleFlags value;
		const char* name;
	};

	struct EmitterTrackField
	{
		const char* name;
		FloatTrack ParticleEmitterDefinition::* value;
	};

	constexpr CurveName kCurveNames[] = {
		{ CurveType::Linear, "Linear" },
		{ CurveType::EaseIn, "EaseIn" },
		{ CurveType::EaseOut, "EaseOut" },
		{ CurveType::EaseInOut, "EaseInOut" },
		{ CurveType::EaseInOutWeak, "EaseInOutWeak" },
		{ CurveType::FastInOut, "FastInOut" },
		{ CurveType::FastInOutWeak, "FastInOutWeak" },
		{ CurveType::Bounce, "Bounce" },
		{ CurveType::BounceFastMiddle, "BounceFastMiddle" },
		{ CurveType::BounceSlowMiddle, "BounceSlowMiddle" },
		{ CurveType::SinWave, "SinWave" },
		{ CurveType::EaseSinWave, "EaseSinWave" }
	};

	class XmlStringWriter final : public pugi::xml_writer
	{
	public:
		explicit XmlStringWriter(std::string& output)
			: _output(output)
		{
		}

		virtual void write(const void* data, wx::Size size) override
		{
			_output.append(static_cast<const char*>(data), size);
		}

	private:
		std::string& _output;
	};

	class TrackParser final
	{
	public:
		explicit TrackParser(std::string_view text)
			: _text(text),
			_current(_text.c_str())
		{
		}

		bool Parse(FloatTrack& track, std::string& error)
		{
			track.nodes.clear();
			_skip_space();
			while (*_current != '\0')
			{
				if (track.nodes.size() >= pop::particles::kTodMaxTrackNodes)
				{
					error = pop::ML("xml.track_too_many");
					return false;
				}

				FloatTrackNode node{};
				node.time = -1.0f;
				node.curve = CurveType::Linear;
				node.distribution = CurveType::Linear;

				if (*_current == '[')
				{
					++_current;
					if (!_parse_float(node.low_value))
						return _fail(pop::ML("xml.track_invalid_range_min"), error);

					_skip_space();
					if (*_current == ']')
					{
						node.high_value = node.low_value;
						++_current;
					}
					else
					{
						bool foundDistribution = false;
						if (!_parse_curve(node.distribution, foundDistribution))
						{
							return _fail(pop::ML("xml.track_invalid_distribution"), error);
						}
						if (!_parse_float(node.high_value))
							return _fail(pop::ML("xml.track_invalid_range_max"), error);
						_skip_space();
						if (*_current != ']')
							return _fail(pop::ML("xml.track_missing_range_end"), error);
						++_current;
					}
				}
				else
				{
					if (!_parse_float(node.low_value))
						return _fail(pop::ML("xml.track_invalid_value"), error);
					node.high_value = node.low_value;
				}

				_skip_space();
				if (*_current == ',')
				{
					++_current;
					if (!_parse_float(node.time))
						return _fail(pop::ML("xml.track_invalid_time"), error);
					node.time *= 0.01f;
				}

				bool foundCurve = false;
				if (!_parse_curve(node.curve, foundCurve))
					return _fail(pop::ML("xml.track_unknown_curve"), error);

				track.nodes.push_back(node);
				_skip_space();
			}

			if (track.nodes.empty())
				return _fail(pop::ML("xml.track_empty"), error);

			wx::Float32 previousTime = 0.0f;
			for (wx::Size index = 0; index < track.nodes.size(); ++index)
			{
				FloatTrackNode& node = track.nodes[index];
				if (node.time < 0.0f)
				{
					node.time = _find_evenly_spaced_time(track, index);
				}
				else if (previousTime > node.time)
				{
					return _fail(pop::ML("xml.track_time_order"), error);
				}
				else if (node.time > 1.0f)
				{
					return _fail(pop::ML("xml.track_time_over_100"), error);
				}
				previousTime = node.time;
			}

			error.clear();
			return true;
		}

	private:
		void _skip_space()
		{
			while (*_current == ' ' || *_current == '\t' ||
				*_current == '\r' || *_current == '\n')
			{
				++_current;
			}
		}

		bool _parse_float(wx::Float32& value)
		{
			_skip_space();
			errno = 0;
			char* end = nullptr;
			const wx::Float32 parsedValue = std::strtof(_current, &end);
			if (end == _current || errno == ERANGE || !std::isfinite(parsedValue))
				return false;

			_current = end;
			value = parsedValue;
			return true;
		}

		bool _parse_curve(CurveType& curve, bool& found)
		{
			_skip_space();
			found = false;
			if (!((*_current >= 'A' && *_current <= 'Z') ||
				(*_current >= 'a' && *_current <= 'z')))
			{
				return true;
			}

			const char* begin = _current;
			while ((*_current >= 'A' && *_current <= 'Z') ||
				(*_current >= 'a' && *_current <= 'z'))
			{
				++_current;
			}

			const std::string_view name(begin, _current - begin);
			for (const CurveName& option : kCurveNames)
			{
				if (_same_name(name, option.name))
				{
					curve = option.value;
					found = true;
					return true;
				}
			}
			return false;
		}

		static wx::Float32 _find_evenly_spaced_time(
			const FloatTrack& track,
			wx::Size currentIndex)
		{
			if (currentIndex == 0)
				return 0.0f;
			if (currentIndex + 1 == track.nodes.size())
				return 1.0f;

			wx::Size targetIndex = track.nodes.size() - 1;
			wx::Float32 nextTime = 1.0f;
			for (wx::Size nextIndex = currentIndex + 1;
				nextIndex < track.nodes.size();
				++nextIndex)
			{
				if (track.nodes[nextIndex].time >= 0.0f)
				{
					targetIndex = nextIndex;
					nextTime = track.nodes[nextIndex].time;
				}
			}

			const wx::Size previousIndex = currentIndex - 1;
			const wx::Float32 factor = 1.0f /
				static_cast<wx::Float32>(targetIndex - previousIndex);
			const wx::Float32 previousTime = track.nodes[previousIndex].time;
			return factor * (nextTime - previousTime) + previousTime;
		}

		static bool _same_name(std::string_view lhs, std::string_view rhs)
		{
			if (lhs.size() != rhs.size())
				return false;
			for (wx::Size index = 0; index < lhs.size(); ++index)
			{
				char left = lhs[index];
				char right = rhs[index];
				if (left >= 'A' && left <= 'Z')
					left = static_cast<char>(left - 'A' + 'a');
				if (right >= 'A' && right <= 'Z')
					right = static_cast<char>(right - 'A' + 'a');
				if (left != right)
					return false;
			}
			return true;
		}

		static bool _fail(std::string_view message, std::string& error)
		{
			error.assign(message.data(), message.size());
			return false;
		}

	private:
		std::string _text;
		const char* _current;
	};

	constexpr EmitterTypeName kEmitterTypes[] = {
		{ EmitterType::Circle, "Circle" },
		{ EmitterType::Box, "Box" },
		{ EmitterType::BoxPath, "BoxPath" },
		{ EmitterType::CirclePath, "CirclePath" },
		{ EmitterType::CircleEvenSpacing, "CircleEvenSpacing" }
	};

	constexpr FieldTypeName kFieldTypes[] = {
		{ ParticleFieldType::Friction, "Friction" },
		{ ParticleFieldType::Acceleration, "Acceleration" },
		{ ParticleFieldType::Attractor, "Attractor" },
		{ ParticleFieldType::MaxVelocity, "MaxVelocity" },
		{ ParticleFieldType::Velocity, "Velocity" },
		{ ParticleFieldType::Position, "Position" },
		{ ParticleFieldType::SystemPosition, "SystemPosition" },
		{ ParticleFieldType::GroundConstraint, "GroundConstraint" },
		{ ParticleFieldType::Shake, "Shake" },
		{ ParticleFieldType::Circle, "Circle" },
		{ ParticleFieldType::Away, "Away" }
	};

	constexpr ParticleFlagName kParticleFlags[] = {
		{ pop::particles::RandomLaunchSpin, "RandomLaunchSpin" },
		{ pop::particles::AlignLaunchSpin, "AlignLaunchSpin" },
		{ pop::particles::AlignToPixel, "AlignToPixel" },
		{ pop::particles::ParticleLoops, "ParticleLoops" },
		{ pop::particles::SystemLoops, "SystemLoops" },
		{ pop::particles::ParticlesDontFollow, "ParticlesDontFollow" },
		{ pop::particles::RandomStartTime, "RandomStartTime" },
		{ pop::particles::DieIfOverloaded, "DieIfOverloaded" },
		{ pop::particles::Additive, "Additive" },
		{ pop::particles::FullScreen, "FullScreen" },
		{ pop::particles::SoftwareOnly, "SoftwareOnly" },
		{ pop::particles::HardwareOnly, "HardwareOnly" }
	};

	constexpr EmitterTrackField kEmitterTracksBeforeFields[] = {
		{ "SystemDuration", &ParticleEmitterDefinition::system_duration },
		{ "CrossFadeDuration", &ParticleEmitterDefinition::cross_fade_duration },
		{ "SpawnRate", &ParticleEmitterDefinition::spawn_rate },
		{ "SpawnMinActive", &ParticleEmitterDefinition::spawn_min_active },
		{ "SpawnMaxActive", &ParticleEmitterDefinition::spawn_max_active },
		{ "SpawnMaxLaunched", &ParticleEmitterDefinition::spawn_max_launched },
		{ "EmitterRadius", &ParticleEmitterDefinition::emitter_radius },
		{ "EmitterOffsetX", &ParticleEmitterDefinition::emitter_offset_x },
		{ "EmitterOffsetY", &ParticleEmitterDefinition::emitter_offset_y },
		{ "EmitterBoxX", &ParticleEmitterDefinition::emitter_box_x },
		{ "EmitterBoxY", &ParticleEmitterDefinition::emitter_box_y },
		{ "EmitterPath", &ParticleEmitterDefinition::emitter_path },
		{ "EmitterSkewX", &ParticleEmitterDefinition::emitter_skew_x },
		{ "EmitterSkewY", &ParticleEmitterDefinition::emitter_skew_y },
		{ "ParticleDuration", &ParticleEmitterDefinition::particle_duration },
		{ "SystemRed", &ParticleEmitterDefinition::system_red },
		{ "SystemGreen", &ParticleEmitterDefinition::system_green },
		{ "SystemBlue", &ParticleEmitterDefinition::system_blue },
		{ "SystemAlpha", &ParticleEmitterDefinition::system_alpha },
		{ "SystemBrightness", &ParticleEmitterDefinition::system_brightness },
		{ "LaunchSpeed", &ParticleEmitterDefinition::launch_speed },
		{ "LaunchAngle", &ParticleEmitterDefinition::launch_angle }
	};

	constexpr EmitterTrackField kEmitterTracksAfterFields[] = {
		{ "ParticleRed", &ParticleEmitterDefinition::particle_red },
		{ "ParticleGreen", &ParticleEmitterDefinition::particle_green },
		{ "ParticleBlue", &ParticleEmitterDefinition::particle_blue },
		{ "ParticleAlpha", &ParticleEmitterDefinition::particle_alpha },
		{ "ParticleBrightness", &ParticleEmitterDefinition::particle_brightness },
		{ "ParticleSpinAngle", &ParticleEmitterDefinition::particle_spin_angle },
		{ "ParticleSpinSpeed", &ParticleEmitterDefinition::particle_spin_speed },
		{ "ParticleScale", &ParticleEmitterDefinition::particle_scale },
		{ "ParticleStretch", &ParticleEmitterDefinition::particle_stretch },
		{ "CollisionReflect", &ParticleEmitterDefinition::collision_reflect },
		{ "CollisionSpin", &ParticleEmitterDefinition::collision_spin },
		{ "ClipTop", &ParticleEmitterDefinition::clip_top },
		{ "ClipBottom", &ParticleEmitterDefinition::clip_bottom },
		{ "ClipLeft", &ParticleEmitterDefinition::clip_left },
		{ "ClipRight", &ParticleEmitterDefinition::clip_right },
		{ "AnimationRate", &ParticleEmitterDefinition::animation_rate }
	};

	bool SameName(std::string_view lhs, std::string_view rhs)
	{
		if (lhs.size() != rhs.size())
			return false;
		for (wx::Size index = 0; index < lhs.size(); ++index)
		{
			char left = lhs[index];
			char right = rhs[index];
			if (left >= 'A' && left <= 'Z')
				left = static_cast<char>(left - 'A' + 'a');
			if (right >= 'A' && right <= 'Z')
				right = static_cast<char>(right - 'A' + 'a');
			if (left != right)
				return false;
		}
		return true;
	}

	std::string_view Trim(std::string_view text)
	{
		while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
			text.front() == '\r' || text.front() == '\n'))
		{
			text.remove_prefix(1);
		}
		while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
			text.back() == '\r' || text.back() == '\n'))
		{
			text.remove_suffix(1);
		}
		return text;
	}

	bool ParseInt(std::string_view text, wx::Int32& value)
	{
		text = Trim(text);
		if (text.empty())
			return false;
		if (text.front() == '+')
			text.remove_prefix(1);
		if (text.empty())
			return false;

		const char* begin = text.data();
		const char* end = begin + text.size();
		const auto result = std::from_chars(begin, end, value);
		return result.ec == std::errc{} && result.ptr == end;
	}

	std::string FormatFloat(wx::Float32 value)
	{
		if (value == 0.0f)
			return "0";

		char buffer[64]{};
		const auto result = std::to_chars(
			buffer,
			buffer + sizeof(buffer),
			value);
		if (result.ec != std::errc{})
			return "0";
		return std::string(buffer, result.ptr);
	}

	const char* FindCurveName(CurveType value)
	{
		for (const CurveName& option : kCurveNames)
			if (option.value == value)
				return option.name;
		return nullptr;
	}

	template<typename Option, wx::Size Size, typename Value>
	bool ParseNamedValue(
		std::string_view text,
		const Option (&options)[Size],
		Value& value)
	{
		text = Trim(text);
		for (const Option& option : options)
		{
			if (SameName(text, option.name))
			{
				value = option.value;
				return true;
			}
		}
		return false;
	}

	template<typename Option, wx::Size Size, typename Value>
	const char* FindNamedValue(
		Value value,
		const Option (&options)[Size])
	{
		for (const Option& option : options)
			if (option.value == value)
				return option.name;
		return nullptr;
	}

	const EmitterTrackField* FindEmitterTrack(std::string_view name)
	{
		for (const EmitterTrackField& field : kEmitterTracksBeforeFields)
			if (SameName(name, field.name))
				return &field;
		for (const EmitterTrackField& field : kEmitterTracksAfterFields)
			if (SameName(name, field.name))
				return &field;
		return nullptr;
	}

	bool IsTrackSet(const FloatTrack& track)
	{
		return !track.nodes.empty() &&
			!(track.nodes.size() == 1 &&
				track.nodes.front().curve == CurveType::Constant);
	}

	bool FormatTrack(
		const FloatTrack& track,
		std::string& text,
		std::string& error)
	{
		if (track.nodes.empty() ||
			track.nodes.size() > pop::particles::kTodMaxTrackNodes)
		{
			error = pop::ML("xml.track_count");
			return false;
		}

		text.clear();
		wx::Float32 previousTime = 0.0f;
		for (wx::Size index = 0; index < track.nodes.size(); ++index)
		{
			const FloatTrackNode& node = track.nodes[index];
			if (!std::isfinite(node.time) || !std::isfinite(node.low_value) ||
				!std::isfinite(node.high_value) || node.time < 0.0f ||
				node.time > 1.0f || (index > 0 && node.time < previousTime))
			{
				error = pop::ML("xml.track_invalid_node");
				return false;
			}

			if (index > 0)
				text += ' ';

			if (node.low_value == node.high_value)
			{
				text += FormatFloat(node.low_value);
			}
			else
			{
				text += '[';
				text += FormatFloat(node.low_value);
				if (node.distribution != CurveType::Linear)
				{
					const char* distributionName = FindCurveName(node.distribution);
					if (distributionName == nullptr)
					{
						error = pop::ML("xml.track_unsupported_distribution");
						return false;
					}
					text += ' ';
					text += distributionName;
				}
				text += ' ';
				text += FormatFloat(node.high_value);
				text += ']';
			}

			if (track.nodes.size() > 1)
			{
				text += ',';
				text += FormatFloat(node.time * 100.0f);
			}

			if (index + 1 < track.nodes.size() &&
				node.curve != CurveType::Linear)
			{
				const char* curveName = FindCurveName(node.curve);
				if (curveName == nullptr)
				{
					error = pop::ML("xml.track_unsupported_curve");
					return false;
				}
				text += ' ';
				text += curveName;
			}

			previousTime = node.time;
		}

		error.clear();
		return true;
	}

	void AppendText(
		pugi::xml_node parent,
		const char* name,
		std::string_view value)
	{
		pugi::xml_node node = parent.append_child(name);
		const std::string text(value);
		node.text().set(text.c_str());
	}

	void AppendInt(pugi::xml_node parent, const char* name, wx::Int32 value)
	{
		pugi::xml_node node = parent.append_child(name);
		node.text().set(value);
	}

	bool AppendTrack(
		pugi::xml_node parent,
		const EmitterTrackField& field,
		const ParticleEmitterDefinition& definition,
		std::string& error)
	{
		const FloatTrack& track = definition.*field.value;
		if (!IsTrackSet(track))
			return true;

		std::string text;
		if (!FormatTrack(track, text, error))
		{
			error = pop::MLF("xml.field_error", pop::ML(field.name), error.c_str());
			return false;
		}
		AppendText(parent, field.name, text);
		return true;
	}

	bool AppendField(
		pugi::xml_node emitterNode,
		const char* elementName,
		const ParticleFieldDefinition& definition,
		std::string& error)
	{
		pugi::xml_node fieldNode = emitterNode.append_child(elementName);
		if (definition.type != ParticleFieldType::Invalid)
		{
			const char* typeName = FindNamedValue(definition.type, kFieldTypes);
			if (typeName == nullptr)
			{
				error = pop::ML("xml.field_unsupported_type");
				return false;
			}
			AppendText(fieldNode, "FieldType", typeName);
		}

		std::string text;
		if (IsTrackSet(definition.x))
		{
			if (!FormatTrack(definition.x, text, error))
			{
				error = pop::MLF("xml.field_axis_error", "X", error.c_str());
				return false;
			}
			AppendText(fieldNode, "X", text);
		}
		if (IsTrackSet(definition.y))
		{
			if (!FormatTrack(definition.y, text, error))
			{
				error = pop::MLF("xml.field_axis_error", "Y", error.c_str());
				return false;
			}
			AppendText(fieldNode, "Y", text);
		}
		return true;
	}

	bool ParseTrackNode(
		pugi::xml_node node,
		FloatTrack& track,
		std::string& error)
	{
		const std::string_view text = node.text().as_string();
		TrackParser parser(text);
		return parser.Parse(track, error);
	}

	bool ParseField(
		pugi::xml_node fieldNode,
		ParticleFieldDefinition& definition,
		std::string& error)
	{
		definition = {};
		for (pugi::xml_node node : fieldNode.children())
		{
			if (node.type() != pugi::node_element)
				continue;

			const std::string_view name = node.name();
			if (SameName(name, "FieldType"))
			{
				if (!ParseNamedValue(
					node.text().as_string(),
					kFieldTypes,
					definition.type))
				{
					error = pop::ML("xml.field_unknown_type");
					return false;
				}
			}
			else if (SameName(name, "X"))
			{
				if (!ParseTrackNode(node, definition.x, error))
					return false;
			}
			else if (SameName(name, "Y"))
			{
				if (!ParseTrackNode(node, definition.y, error))
					return false;
			}
			else
			{
				error = pop::MLF("xml.field_unknown_element", node.name());
				return false;
			}
		}
		return true;
	}

	bool ParseEmitter(
		pugi::xml_node emitterNode,
		ParticleEmitterDefinition& definition,
		std::string& error)
	{
		definition = {};
		for (pugi::xml_node node : emitterNode.children())
		{
			if (node.type() != pugi::node_element)
				continue;

			const std::string_view name = node.name();
			const std::string_view text = node.text().as_string();
			if (SameName(name, "Image"))
			{
				const std::string_view trimmedText = Trim(text);
				definition.image.assign(trimmedText.data(), trimmedText.size());
				continue;
			}

			wx::Int32 intValue = 0;
			if (SameName(name, "ImageRow"))
			{
				if (!ParseInt(text, definition.image_row))
				{
					error = pop::ML("xml.invalid_image_row");
					return false;
				}
				continue;
			}
			if (SameName(name, "ImageCol"))
			{
				if (!ParseInt(text, definition.image_col))
				{
					error = pop::ML("xml.invalid_image_col");
					return false;
				}
				continue;
			}
			if (SameName(name, "ImageFrames"))
			{
				if (!ParseInt(text, definition.image_frames) ||
					definition.image_frames < 1)
				{
					error = pop::ML("xml.image_frames_min");
					return false;
				}
				continue;
			}
			if (SameName(name, "Animated"))
			{
				if (!ParseInt(text, intValue))
				{
					error = pop::ML("xml.invalid_animated");
					return false;
				}
				definition.animated = intValue != 0;
				continue;
			}

			bool matchedFlag = false;
			for (const ParticleFlagName& option : kParticleFlags)
			{
				if (!SameName(name, option.name))
					continue;
				if (!ParseInt(text, intValue))
				{
					error = pop::MLF("xml.invalid_option", pop::ML(option.name));
					return false;
				}
				wx::Uint32 flags = static_cast<wx::Uint32>(definition.flags);
				const wx::Uint32 bit = static_cast<wx::Uint32>(option.value);
				flags = intValue != 0 ? flags | bit : flags & ~bit;
				definition.flags = static_cast<ParticleFlags>(flags);
				matchedFlag = true;
				break;
			}
			if (matchedFlag)
				continue;

			if (SameName(name, "EmitterType"))
			{
				if (!ParseNamedValue(text, kEmitterTypes, definition.emitter_type))
				{
					error = pop::ML("xml.unknown_emitter_type");
					return false;
				}
				continue;
			}
			if (SameName(name, "Name"))
			{
				definition.name.assign(text.data(), text.size());
				continue;
			}
			if (SameName(name, "OnDuration"))
			{
				definition.on_duration.assign(text.data(), text.size());
				continue;
			}

			if (const EmitterTrackField* field = FindEmitterTrack(name))
			{
				if (!ParseTrackNode(node, definition.*field->value, error))
				{
					error = pop::MLF("xml.field_error", pop::ML(field->name), error.c_str());
					return false;
				}
				continue;
			}

			if (SameName(name, "Field") || SameName(name, "SystemField"))
			{
				std::vector<ParticleFieldDefinition>& fields = SameName(name, "Field")
					? definition.particle_fields
					: definition.system_fields;
				if (fields.size() >= pop::particles::kTodMaxParticleFields)
				{
					error = pop::MLF("xml.entries_too_many", pop::ML(node.name()));
					return false;
				}
				ParticleFieldDefinition field{};
				if (!ParseField(node, field, error))
					return false;
				fields.push_back(std::move(field));
				continue;
			}

			error = pop::MLF("xml.unknown_emitter_element", node.name());
			return false;
		}
		return true;
	}
}

namespace pop::particles
{
	bool SerializeParticleSystemXml(
		const ParticleSystemDefinition& definition,
		std::string& xml,
		std::string& error)
	{
		pugi::xml_document document;
		for (wx::Size emitterIndex = 0;
			emitterIndex < definition.emitters.size();
			++emitterIndex)
		{
			const ParticleEmitterDefinition& emitter =
				definition.emitters[emitterIndex];
			if (emitter.particle_fields.size() > kTodMaxParticleFields ||
				emitter.system_fields.size() > kTodMaxParticleFields)
			{
				error = pop::MLF("xml.emitter_fields_too_many", emitterIndex + 1);
				return false;
			}

			pugi::xml_node emitterNode = document.append_child("Emitter");
			if (!emitter.image.empty())
				AppendText(emitterNode, "Image", emitter.image);
			if (emitter.image_row != 0)
				AppendInt(emitterNode, "ImageRow", emitter.image_row);
			if (emitter.image_col != 0)
				AppendInt(emitterNode, "ImageCol", emitter.image_col);
			if (emitter.image_frames != 1)
				AppendInt(emitterNode, "ImageFrames", emitter.image_frames);
			if (emitter.animated)
				AppendInt(emitterNode, "Animated", 1);

			for (const ParticleFlagName& option : kParticleFlags)
				if (HasFlag(emitter.flags, option.value))
					AppendInt(emitterNode, option.name, 1);

			const char* emitterTypeName = FindNamedValue(
				emitter.emitter_type,
				kEmitterTypes);
			if (emitterTypeName == nullptr)
			{
				error = pop::MLF("xml.emitter_unsupported_type", emitterIndex + 1);
				return false;
			}
			AppendText(emitterNode, "EmitterType", emitterTypeName);
			if (!emitter.name.empty())
				AppendText(emitterNode, "Name", emitter.name);

			for (const EmitterTrackField& field : kEmitterTracksBeforeFields)
			{
				if (!AppendTrack(emitterNode, field, emitter, error))
				{
					error = pop::MLF(
						"xml.emitter_error",
						emitterIndex + 1,
						error.c_str());
					return false;
				}
			}

			if (!emitter.on_duration.empty())
				AppendText(emitterNode, "OnDuration", emitter.on_duration);

			for (const ParticleFieldDefinition& field : emitter.particle_fields)
			{
				if (!AppendField(emitterNode, "Field", field, error))
					return false;
			}
			for (const ParticleFieldDefinition& field : emitter.system_fields)
			{
				if (!AppendField(emitterNode, "SystemField", field, error))
					return false;
			}

			for (const EmitterTrackField& field : kEmitterTracksAfterFields)
			{
				if (!AppendTrack(emitterNode, field, emitter, error))
				{
					error = pop::MLF(
						"xml.emitter_error",
						emitterIndex + 1,
						error.c_str());
					return false;
				}
			}
		}

		xml.clear();
		XmlStringWriter writer(xml);
		document.save(
			writer,
			"  ",
			pugi::format_indent | pugi::format_no_declaration,
			pugi::encoding_utf8);
		error.clear();
		return true;
	}

	bool DeserializeParticleSystemXml(
		std::string_view xml,
		ParticleSystemDefinition& definition,
		std::string& error)
	{
		if (Trim(xml).empty())
		{
			definition = {};
			error.clear();
			return true;
		}

		pugi::xml_document document;
		const pugi::xml_parse_result result = document.load_buffer(
			xml.data(),
			xml.size(),
			pugi::parse_default | pugi::parse_fragment,
			pugi::encoding_auto);
		if (!result)
		{
			error = pop::MLF(
				"xml.invalid_document",
				static_cast<std::size_t>(result.offset),
				result.description());
			return false;
		}

		ParticleSystemDefinition parsedDefinition;
		for (pugi::xml_node node : document.children())
		{
			if (node.type() != pugi::node_element)
				continue;
			if (!SameName(node.name(), "Emitter"))
			{
				error = pop::MLF("xml.unknown_document_element", node.name());
				return false;
			}

			ParticleEmitterDefinition emitter{};
			if (!ParseEmitter(node, emitter, error))
			{
				error = pop::MLF(
					"xml.emitter_error",
					parsedDefinition.emitters.size() + 1,
					error.c_str());
				return false;
			}
			parsedDefinition.emitters.push_back(std::move(emitter));
		}

		definition = std::move(parsedDefinition);
		error.clear();
		return true;
	}
}
