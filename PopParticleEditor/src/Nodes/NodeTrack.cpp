#include "../../include/Nodes/NodeTrack.h"
#include "../../include/Nodes/NodeColors.h"
#include "../../include/Fonts/IconsFontAwesome7.h"
#include "../../include/Particles/ParticleJson.h"
#include "../../include/PopState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace
{
	using pop::particles::CurveType;
	using pop::particles::FloatTrack;
	using pop::particles::FloatTrackNode;

	struct GraphRect
	{
		ImVec2 min;
		ImVec2 max;

		wx::Float32 Width() const { return max.x - min.x; }
		wx::Float32 Height() const { return max.y - min.y; }
		ImVec2 Center() const { return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f }; }
	};

	struct CurveOption
	{
		CurveType type;
		const char* name;
	};

	constexpr CurveOption kCurveOptions[] = {
		{ CurveType::Constant, "Constant" },
		{ CurveType::Linear, "Linear" },
		{ CurveType::EaseIn, "Ease In" },
		{ CurveType::EaseOut, "Ease Out" },
		{ CurveType::EaseInOut, "Ease In Out" },
		{ CurveType::EaseInOutWeak, "Ease In Out Weak" },
		{ CurveType::FastInOut, "Fast In Out" },
		{ CurveType::FastInOutWeak, "Fast In Out Weak" },
		{ CurveType::Bounce, "Bounce" },
		{ CurveType::BounceFastMiddle, "Bounce Fast Middle" },
		{ CurveType::BounceSlowMiddle, "Bounce Slow Middle" },
		{ CurveType::SinWave, "Sin Wave" },
		{ CurveType::EaseSinWave, "Ease Sin Wave" }
	};

	wx::Float32 CurveS(wx::Float32 t)
	{
		return 3.0f * t * t - 2.0f * t * t * t;
	}

	wx::Float32 CurveInvQuadS(wx::Float32 t)
	{
		if (t <= 0.5f)
		{
			const wx::Float32 value = t * 2.0f;
			return (2.0f * value - value * value) * 0.5f;
		}
		const wx::Float32 value = (t - 0.5f) * 2.0f;
		return value * value * 0.5f + 0.5f;
	}

	wx::Float32 CurvePosition(wx::Float32 t, CurveType curve)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		switch (curve)
		{
		case CurveType::Constant: return 0.0f;
		case CurveType::EaseIn: return t * t;
		case CurveType::EaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
		case CurveType::EaseInOut: return CurveS(CurveS(t));
		case CurveType::EaseInOutWeak: return CurveS(t);
		case CurveType::FastInOut: return CurveInvQuadS(CurveInvQuadS(t));
		case CurveType::FastInOutWeak: return CurveInvQuadS(t);
		case CurveType::Bounce: return 1.0f - std::abs(2.0f * t - 1.0f);
		case CurveType::BounceFastMiddle:
		{
			const wx::Float32 bounce = CurvePosition(t, CurveType::Bounce);
			return bounce * bounce;
		}
		case CurveType::BounceSlowMiddle:
		{
			const wx::Float32 bounce = CurvePosition(t, CurveType::Bounce);
			return 2.0f * bounce - bounce * bounce;
		}
		case CurveType::SinWave: return std::sin(2.0f * 3.14159265f * t);
		case CurveType::EaseSinWave: return std::sin(2.0f * 3.14159265f * CurveS(t));
		default: return t;
		}
	}

	wx::Float32 EvaluateNodeValue(
		const FloatTrackNode& node,
		wx::Float32 interpolation)
	{
		const wx::Float32 position = CurvePosition(
			interpolation,
			node.distribution);
		return node.low_value + (node.high_value - node.low_value) * position;
	}

	wx::Float32 EvaluateTrack(
		const FloatTrack& track,
		wx::Float32 time,
		wx::Float32 interpolation)
	{
		if (track.nodes.empty())
			return 0.0f;

		if (time < track.nodes.front().time)
			return EvaluateNodeValue(track.nodes.front(), interpolation);

		for (wx::Size index = 1; index < track.nodes.size(); ++index)
		{
			const FloatTrackNode& next = track.nodes[index];
			if (time > next.time)
				continue;

			const FloatTrackNode& current = track.nodes[index - 1];
			const wx::Float32 duration = next.time - current.time;
			const wx::Float32 fraction = duration > 0.0f
				? (time - current.time) / duration
				: 1.0f;
			const wx::Float32 left = EvaluateNodeValue(current, interpolation);
			const wx::Float32 right = EvaluateNodeValue(next, interpolation);
			const wx::Float32 position = CurvePosition(fraction, current.curve);
			return left + (right - left) * position;
		}

		return EvaluateNodeValue(track.nodes.back(), interpolation);
	}

	void EvaluateTrackRange(
		const FloatTrack& track,
		wx::Float32 time,
		wx::Float32& minimum,
		wx::Float32& maximum)
	{
		constexpr wx::Int32 distributionSamples = 24;
		minimum = EvaluateTrack(track, time, 0.0f);
		maximum = minimum;
		for (wx::Int32 sample = 1; sample <= distributionSamples; ++sample)
		{
			const wx::Float32 interpolation =
				static_cast<wx::Float32>(sample) / distributionSamples;
			const wx::Float32 value = EvaluateTrack(
				track,
				time,
				interpolation);
			minimum = std::min(minimum, value);
			maximum = std::max(maximum, value);
		}
	}

	wx::Size FindTrackSegment(const FloatTrack& track, wx::Float32 time)
	{
		if (track.nodes.empty() || time <= track.nodes.front().time)
			return 0;
		for (wx::Size index = 1; index < track.nodes.size(); ++index)
			if (time <= track.nodes[index].time)
				return index - 1;
		return track.nodes.size() - 1;
	}

	ImVec2 TrackToScreen(const GraphRect& rect, wx::Float32 time, wx::Float32 value, wx::Float32 view_min, wx::Float32 view_max)
	{
		const wx::Float32 normalized_value = (value - view_min) / (view_max - view_min);
		return { rect.min.x + time * rect.Width(), rect.max.y - normalized_value * rect.Height() };
	}

	void ScreenToTrack(const GraphRect& rect, const ImVec2& point, wx::Float32 view_min, wx::Float32 view_max, wx::Float32& time, wx::Float32& value)
	{
		time = std::clamp((point.x - rect.min.x) / rect.Width(), 0.0f, 1.0f);
		const wx::Float32 normalized_value = std::clamp((rect.max.y - point.y) / rect.Height(), 0.0f, 1.0f);
		value = view_min + normalized_value * (view_max - view_min);
	}
}

namespace pop
{
	NodeTrackNode::NodeTrackNode()
		: Node(),
		_track({ {
			{ 0.0f, 0.0f, 0.0f, particles::CurveType::Linear, particles::CurveType::Linear },
			{ 1.0f, 1.0f, 1.0f, particles::CurveType::Linear, particles::CurveType::Linear }
		} })
	{
		_name = "NodeTrack";
		_serialization_type = "NodeTrack";
		_header_color = node_colors::header::NodeTrack;
	}

	const particles::FloatTrack& NodeTrackNode::_get_track() const noexcept
	{
		return _track;
	}

	particles::FloatTrack& NodeTrackNode::_get_track() noexcept
	{
		return _track;
	}

	void NodeTrackNode::_add_node(wx::Float32 time, wx::Float32 value)
	{
		if (_track.nodes.size() >= particles::kTodMaxTrackNodes)
			return;

		time = std::clamp(time, 0.0f, 1.0f);

		_track.nodes.push_back(
			{
				time,
				value,
				value,
				particles::CurveType::Linear,
				particles::CurveType::Linear });

		// 重新排序
		std::sort(_track.nodes.begin(), _track.nodes.end(), [](const auto& lhs, const auto& rhs) { return lhs.time < rhs.time; });

		for (NodeIndex index = 0; index < static_cast<NodeIndex>(_track.nodes.size()); ++index)
			if (std::abs(_track.nodes[index].time - time) < 0.0001f)
				_selected_node_index = index; // 得到排序后的index（???）
		_selected_high_value = false;
		_dragging_vertical_only = false;
		_splitting_range = false;
		_split_anchor_value = 0.0f;
	}

	void NodeTrackNode::_fit_view_to_track()
	{
		if (_track.nodes.empty())
		{
			_view_min = -1.0f;
			_view_max = 1.0f;
			return;
		}

		constexpr wx::Int32 timeSamples = 96;
		wx::Float32 minimum = std::numeric_limits<wx::Float32>::max();
		wx::Float32 maximum = std::numeric_limits<wx::Float32>::lowest();
		auto includeRange = [&minimum, &maximum](
			wx::Float32 rangeMinimum,
			wx::Float32 rangeMaximum)
		{
			minimum = std::min(minimum, rangeMinimum);
			maximum = std::max(maximum, rangeMaximum);
		};

		for (wx::Int32 sample = 0; sample <= timeSamples; ++sample)
		{
			const wx::Float32 time = static_cast<wx::Float32>(sample) /
				timeSamples;
			wx::Float32 rangeMinimum = 0.0f;
			wx::Float32 rangeMaximum = 0.0f;
			EvaluateTrackRange(_track, time, rangeMinimum, rangeMaximum);
			includeRange(rangeMinimum, rangeMaximum);
		}

		for (const FloatTrackNode& node : _track.nodes)
		{
			wx::Float32 rangeMinimum = 0.0f;
			wx::Float32 rangeMaximum = 0.0f;
			EvaluateTrackRange(
				_track,
				node.time,
				rangeMinimum,
				rangeMaximum);
			includeRange(rangeMinimum, rangeMaximum);
		}

		if (!std::isfinite(minimum) || !std::isfinite(maximum))
		{
			_view_min = -1.0f;
			_view_max = 1.0f;
			return;
		}

		const wx::Float32 range = maximum - minimum;
		const wx::Float32 padding = range > 0.0001f
			? std::max(range * 0.1f, 0.01f)
			: std::max(std::abs(maximum) * 0.1f, 0.5f);
		_view_min = minimum - padding;
		_view_max = maximum + padding;
	}

	void NodeTrackNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "NodeTrack";
		output.name = "NodeTrack";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::NodeTrack;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> NodeTrackNode::_clone() const
	{
		auto node = std::make_unique<NodeTrackNode>();
		node->_track = _track;
		node->_view_min = _view_min;
		node->_view_max = _view_max;
		return node;
	}

	nlohmann::json NodeTrackNode::_serialize() const
	{
		return {
			{ "track", _track },
			{ "view_min", _view_min },
			{ "view_max", _view_max }
		};
	}

	void NodeTrackNode::_deserialize(const nlohmann::json& data)
	{
		_track = data.value("track", particles::FloatTrack{});
		_view_min = data.value("view_min", -1.0f);
		_view_max = data.value("view_max", 1.0f);
		if (_view_max - _view_min < 0.01f)
			_view_max = _view_min + 0.01f;
		_selected_node_index = -1;
		_selected_high_value = false;
		_dragging_node = -1;
		_dragging_vertical_only = false;
		_splitting_range = false;
		_split_anchor_value = 0.0f;
	}

	void NodeTrackNode::_on_render_workspace()
	{
		const ImVec2 canvasSize(560.0f, 320.0f);
		ImGui::SetNextItemWidth(canvasSize.x - 150.0f);
		ImGui::DragFloatRange2(
			ML("track.y_range"),
			&_view_min,
			&_view_max,
			0.05f,
			-100000.0f,
			100000.0f,
			ML("track.min_format"),
			ML("track.max_format"));
		if (_view_max - _view_min < 0.01f) _view_max = _view_min + 0.01f; // 最大最小值调整

		ImGui::BeginDisabled(_track.nodes.size() >= particles::kTodMaxTrackNodes || _selected_node_index < 0 ||
			_selected_node_index == (_track.nodes.size() - 1));
		if (ImGui::Button(" + ##add_track_node"))
		{
			const FloatTrackNode& start = _track.nodes[_selected_node_index];
			const FloatTrackNode& end = _track.nodes[_selected_node_index + 1];
			const wx::Float32 time = (start.time + end.time) * 0.5f;
			_add_node(time, EvaluateTrack(_track, time, 0.5f));
		}

		_set_item_tooltip(ML("track.add_node"));
		ImGui::EndDisabled();
		ImGui::SameLine();

		const bool canDelete = _selected_node_index >= 0 &&
			_track.nodes.size() > 1;
		auto deleteSelectedNode = [this]()
		{
			_track.nodes.erase(_track.nodes.begin() + _selected_node_index);
			_selected_node_index = std::clamp(_selected_node_index, 0, static_cast<wx::Int32>(_track.nodes.size()) - 1);
			_selected_high_value = false;
			_dragging_node = -1;
			_dragging_vertical_only = false;
			_splitting_range = false;
			_split_anchor_value = 0.0f;
		}; // 删除当前选中的顶点

		ImGui::BeginDisabled(!canDelete);
		if (ImGui::Button(" - ##delete_track_node"))
			deleteSelectedNode();
		_set_item_tooltip(ML("track.delete_node"));
		ImGui::EndDisabled();

		const ImVec2 canvasCursorPosition = ImGui::GetCursorPos();
		const bool hasSelectedNode = _selected_node_index >= 0 &&
			_selected_node_index < static_cast<NodeIndex>(_track.nodes.size());
		if (hasSelectedNode)
		{
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 4.0f);
			FloatTrackNode& selectedNode = _track.nodes[_selected_node_index];
			const bool selectedNodeHasRange =
				std::abs(selectedNode.high_value - selectedNode.low_value) > 0.0001f;
			ImGui::SetNextItemWidth(96.0f);
			if (ImGui::DragFloat(
				"##selected_track_node_x",
				&selectedNode.time,
				0.01f,
				0.0f,
				1.0f,
				ML("track.x_value_format")))
			{
				if (_track.nodes.size() > 1 && _selected_node_index == 0)
					selectedNode.time = std::clamp(
						selectedNode.time,
						0.0f,
						std::max(_track.nodes[1].time - 0.001f, 0.0f));
				else if (_track.nodes.size() > 1 &&
					_selected_node_index + 1 == static_cast<NodeIndex>(_track.nodes.size()))
					selectedNode.time = std::clamp(
						selectedNode.time,
						std::min(_track.nodes[_selected_node_index - 1].time + 0.001f, 1.0f),
						1.0f);
				else if (_track.nodes.size() > 2)
				{
					const wx::Float32 previousTime =
						_track.nodes[_selected_node_index - 1].time;
					const wx::Float32 nextTime =
						_track.nodes[_selected_node_index + 1].time;
					const wx::Float32 margin = std::min(
						0.001f,
						std::max(nextTime - previousTime, 0.0f) * 0.5f);
					selectedNode.time = std::clamp(
						selectedNode.time,
						previousTime + margin,
						nextTime - margin);
				}
			}

			ImGui::SameLine();
			wx::Float32& selectedValue = _selected_high_value
				? selectedNode.high_value
				: selectedNode.low_value;
			ImGui::SetNextItemWidth(112.0f);
			if (ImGui::DragFloat(
				"##selected_track_node_y",
				&selectedValue,
				0.01f,
				-100000.0f,
				100000.0f,
				ML("track.y_value_format")))
			{
				if (selectedNodeHasRange)
				{
					if (_selected_high_value)
						selectedNode.high_value = std::max(
							selectedNode.high_value,
							selectedNode.low_value);
					else
						selectedNode.low_value = std::min(
							selectedNode.low_value,
							selectedNode.high_value);
				}
				else
				{
					selectedNode.low_value = selectedValue;
					selectedNode.high_value = selectedValue;
				}
			}
			ImGui::SetCursorPos(canvasCursorPosition);
		}
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const GraphRect canvas{ origin, { origin.x + canvasSize.x, origin.y + canvasSize.y } };
		const GraphRect graph{
			{ canvas.min.x + 62.0f, canvas.min.y + 14.0f },
			{ canvas.max.x - 14.0f, canvas.max.y - 34.0f }
		};

		ImDrawList* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(canvas.min, canvas.max, IM_COL32(18, 21, 28, 255), 4.0f);
		draw->AddRect(canvas.min, canvas.max, IM_COL32(64, 72, 89, 255), 4.0f);
		draw->AddRectFilled(graph.min, graph.max, IM_COL32(22, 25, 33, 255), 4.0f);
		draw->AddRect(graph.min, graph.max, IM_COL32(76, 84, 102, 255), 4.0f);
		for (wx::Int32 i = 0; i <= 4; ++i)
		{
			const wx::Float32 x = graph.min.x + graph.Width() * i / 4.0f;
			const wx::Float32 y = graph.min.y + graph.Height() * i / 4.0f;
			draw->AddLine({ x, graph.min.y }, { x, graph.max.y }, IM_COL32(52, 58, 72, 255));
			draw->AddLine({ graph.min.x, y }, { graph.max.x, y }, IM_COL32(52, 58, 72, 255));

			char xLabel[16]{};
			std::snprintf(xLabel, sizeof(xLabel), "%.2f", i / 4.0f);
			const ImVec2 xLabelSize = ImGui::CalcTextSize(xLabel);
			draw->AddText({ x - xLabelSize.x * 0.5f, graph.max.y + 7.0f }, IM_COL32(170, 178, 195, 255), xLabel);

			char yLabel[24]{};
			const wx::Float32 yValue = _view_max - (_view_max - _view_min) * i / 4.0f;
			std::snprintf(yLabel, sizeof(yLabel), "%.2f", yValue);
			const ImVec2 yLabelSize = ImGui::CalcTextSize(yLabel);
			draw->AddText({ graph.min.x - yLabelSize.x - 8.0f, y - yLabelSize.y * 0.5f }, IM_COL32(170, 178, 195, 255), yLabel);
		}
		const wx::Float32 xAxisValue = std::clamp(0.0f, _view_min, _view_max);
		const wx::Float32 xAxisY = TrackToScreen(graph, 0.0f, xAxisValue, _view_min, _view_max).y;
		draw->AddLine({ graph.min.x, xAxisY }, { graph.max.x, xAxisY }, IM_COL32(132, 142, 164, 255), 1.5f);
		draw->AddLine(graph.min, { graph.min.x, graph.max.y }, IM_COL32(132, 142, 164, 255), 1.5f);

		if (!_track.nodes.empty())
		{
			constexpr wx::Int32 curveSamples = 96;
			wx::Float32 previousMinimum = 0.0f;
			wx::Float32 previousMaximum = 0.0f;
			EvaluateTrackRange(
				_track,
				0.0f,
				previousMinimum,
				previousMaximum);
			ImVec2 previousMinimumPoint = TrackToScreen(
				graph,
				0.0f,
				previousMinimum,
				_view_min,
				_view_max);
			ImVec2 previousMaximumPoint = TrackToScreen(
				graph,
				0.0f,
				previousMaximum,
				_view_min,
				_view_max);

			draw->PushClipRect(graph.min, graph.max, true);
			for (wx::Int32 sample = 1; sample <= curveSamples; ++sample)
			{
				const wx::Float32 time = static_cast<wx::Float32>(sample) /
					curveSamples;
				wx::Float32 currentMinimum = 0.0f;
				wx::Float32 currentMaximum = 0.0f;
				EvaluateTrackRange(
					_track,
					time,
					currentMinimum,
					currentMaximum);
				const ImVec2 currentMinimumPoint = TrackToScreen(
					graph,
					time,
					currentMinimum,
					_view_min,
					_view_max);
				const ImVec2 currentMaximumPoint = TrackToScreen(
					graph,
					time,
					currentMaximum,
					_view_min,
					_view_max);
				const bool hasRange =
					std::abs(previousMaximum - previousMinimum) > 0.0001f ||
					std::abs(currentMaximum - currentMinimum) > 0.0001f;
				if (hasRange)
				{
					draw->AddQuadFilled(
						previousMinimumPoint,
						currentMinimumPoint,
						currentMaximumPoint,
						previousMaximumPoint,
						IM_COL32(78, 166, 226, 45));
				}

				const wx::Float32 segmentTime = time - 0.5f / curveSamples;
				const bool selectedSegment = _selected_node_index >= 0 &&
					FindTrackSegment(_track, segmentTime) ==
					static_cast<wx::Size>(_selected_node_index);
				const ImU32 curveColor = selectedSegment
					? IM_COL32(255, 213, 94, 255)
					: IM_COL32(132, 204, 255, 255);
				draw->AddLine(
					previousMinimumPoint,
					currentMinimumPoint,
					curveColor,
					2.0f);
				if (hasRange)
				{
					draw->AddLine(
						previousMaximumPoint,
						currentMaximumPoint,
						curveColor,
						2.0f);
				}

				previousMinimum = currentMinimum;
				previousMaximum = currentMaximum;
				previousMinimumPoint = currentMinimumPoint;
				previousMaximumPoint = currentMaximumPoint;
			}

			for (wx::Int32 index = 0;
				index < static_cast<wx::Int32>(_track.nodes.size());
				++index)
			{
				const FloatTrackNode& node = _track.nodes[index];
				const ImVec2 lowPoint = TrackToScreen(
					graph,
					node.time,
					node.low_value,
					_view_min,
					_view_max);
				const ImVec2 highPoint = TrackToScreen(
					graph,
					node.time,
					node.high_value,
					_view_min,
					_view_max);
				const bool selectedLow = index == _selected_node_index &&
					!_selected_high_value;
				const bool selectedHigh = index == _selected_node_index &&
					_selected_high_value;
				if (std::abs(node.high_value - node.low_value) > 0.0001f)
				{
					draw->AddLine(
						lowPoint,
						highPoint,
						IM_COL32(132, 204, 255, 150),
						1.5f);
					draw->AddCircleFilled(
						highPoint,
						selectedHigh ? 6.0f : 5.0f,
						selectedHigh
							? IM_COL32(255, 213, 94, 255)
							: IM_COL32(144, 217, 255, 255));
					draw->AddCircle(
						highPoint,
						7.0f,
						IM_COL32(26, 30, 42, 255),
						16,
						1.5f);
				}
				draw->AddCircleFilled(
					lowPoint,
					selectedLow ? 6.0f : 5.0f,
					selectedLow
						? IM_COL32(255, 213, 94, 255)
						: IM_COL32(232, 242, 255, 255));
				draw->AddCircle(
					lowPoint,
					7.0f,
					IM_COL32(26, 30, 42, 255),
					16,
					1.5f);
			}
			draw->PopClipRect();
		}

		ImGui::SetCursorScreenPos(canvas.min);

		// 检测图被点击
		ImGui::InvisibleButton("##track_graph", { canvas.Width(), canvas.Height() }, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool canvasHovered = ImGui::IsItemHovered();
		const bool canvasTooltipReady = ImGui::IsItemHovered(
			ImGuiHoveredFlags_ForTooltip);
		if (canvasHovered && _selected_node_index >= 0 &&
			ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			ed::EnableShortcuts(false);
			if (canDelete)
				deleteSelectedNode();
		}

		auto findNode = [this, &graph](
			const ImVec2& mousePosition,
			bool& highValue)
		{
			wx::Int32 closest = -1;
			wx::Float32 closestDistanceSquared = 100.0f;
			highValue = false;
			for (wx::Int32 index = 0; index < static_cast<wx::Int32>(_track.nodes.size()); ++index)
			{
				const FloatTrackNode& node = _track.nodes[index];
				const wx::Float32 values[] = { node.low_value, node.high_value };
				const wx::Int32 valueCount =
					std::abs(node.high_value - node.low_value) > 0.0001f ? 2 : 1;
				for (wx::Int32 valueIndex = 0; valueIndex < valueCount; ++valueIndex)
				{
					const ImVec2 point = TrackToScreen(
						graph,
						node.time,
						values[valueIndex],
						_view_min,
						_view_max);
					const wx::Float32 deltaX = mousePosition.x - point.x;
					const wx::Float32 deltaY = mousePosition.y - point.y;
					const wx::Float32 distanceSquared =
						deltaX * deltaX + deltaY * deltaY;
					if (distanceSquared <= closestDistanceSquared)
					{
						closest = index;
						highValue = valueIndex == 1;
						closestDistanceSquared = distanceSquared;
					}
				}
			}
			return closest;
		};
		bool hoveredHighValue = false;
		const wx::Int32 hoveredNodeIndex = canvasHovered
			? findNode(ImGui::GetIO().MousePos, hoveredHighValue)
			: -1;
		_anchor_tooltip.clear();
		if (canvasTooltipReady && hoveredNodeIndex >= 0)
		{
			const FloatTrackNode& hoveredNode = _track.nodes[hoveredNodeIndex];
			const wx::Float32 hoveredValue = hoveredHighValue
				? hoveredNode.high_value
				: hoveredNode.low_value;
			char tooltip[64]{};
			std::snprintf(
				tooltip,
				sizeof(tooltip),
				ML("track.point_tooltip"),
				hoveredNode.time,
				hoveredValue);
			_anchor_tooltip = tooltip;
		}

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			_selected_node_index = hoveredNodeIndex;
			_selected_high_value = hoveredHighValue;
			_dragging_node = _selected_node_index;
			_dragging_vertical_only = false;
			_splitting_range = false;
			if (_dragging_node >= 0)
			{
				const FloatTrackNode& node = _track.nodes[_dragging_node];
				const bool rangeAnchor =
					std::abs(node.high_value - node.low_value) > 0.0001f;
				_splitting_range = ImGui::GetIO().KeyAlt && !rangeAnchor;
				_dragging_vertical_only = ImGui::GetIO().KeyAlt &&
					(rangeAnchor || _splitting_range);
				_split_anchor_value = hoveredHighValue
					? node.high_value
					: node.low_value;
			}
		}
		if (_dragging_node >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			FloatTrackNode& node = _track.nodes[_dragging_node];
			const bool altHeld = ImGui::GetIO().KeyAlt;
			bool rangeAnchor =
				std::abs(node.high_value - node.low_value) > 0.0001f;
			wx::Float32 time = node.time;
			wx::Float32 value = 0.0f;
			ScreenToTrack(
				graph,
				ImGui::GetIO().MousePos,
				_view_min,
				_view_max,
				time,
				value);
			if (!_splitting_range && altHeld && !rangeAnchor)
			{
				_splitting_range = true;
				_split_anchor_value = node.low_value;
			}
			if (_splitting_range && !altHeld)
				_splitting_range = false;

			if (_splitting_range && altHeld)
			{
				if (value >= _split_anchor_value)
				{
					node.low_value = _split_anchor_value;
					node.high_value = value;
					_selected_high_value = true;
				}
				else
				{
					node.low_value = value;
					node.high_value = _split_anchor_value;
					_selected_high_value = false;
				}
			}
			else
			{
				rangeAnchor =
					std::abs(node.high_value - node.low_value) > 0.0001f;
				if (rangeAnchor)
				{
					if (_selected_high_value)
						node.high_value = std::max(value, node.low_value);
					else
						node.low_value = std::min(value, node.high_value);
				}
				else
				{
					node.low_value = value;
					node.high_value = value;
				}
			}
			_dragging_vertical_only = altHeld &&
				(_splitting_range || rangeAnchor);
			if (!_dragging_vertical_only)
			{
				if (_track.nodes.size() == 1)
				{
					node.time = std::clamp(time, 0.0f, 1.0f);
				}
				else if (_dragging_node == 0)
				{
					const wx::Float32 maximumTime = std::max(
						_track.nodes[1].time - 0.001f,
						0.0f);
					node.time = std::clamp(
						time,
						0.0f,
						maximumTime);
				}
				else if (_dragging_node + 1 ==
					static_cast<wx::Int32>(_track.nodes.size()))
				{
					const wx::Float32 minimumTime = std::min(
						_track.nodes[_dragging_node - 1].time + 0.001f,
						1.0f);
					node.time = std::clamp(
						time,
						minimumTime,
						1.0f);
				}
				else
				{
					const wx::Float32 previousTime =
						_track.nodes[_dragging_node - 1].time;
					const wx::Float32 nextTime =
						_track.nodes[_dragging_node + 1].time;
					const wx::Float32 margin = std::min(
						0.001f,
						std::max(nextTime - previousTime, 0.0f) * 0.5f);
					node.time = std::clamp(
						time,
						previousTime + margin,
						nextTime - margin);
				}
			}
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			_dragging_node = -1;
			_dragging_vertical_only = false;
			_splitting_range = false;
			_split_anchor_value = 0.0f;
		}

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			const wx::Int32 nodeIndex = hoveredNodeIndex;
			if (nodeIndex >= 0 && _track.nodes.size() > 1)
			{
				_track.nodes.erase(_track.nodes.begin() + nodeIndex);
				_selected_node_index = std::clamp(_selected_node_index, 0, static_cast<wx::Int32>(_track.nodes.size()) - 1);
				_selected_high_value = false;
				_dragging_vertical_only = false;
				_splitting_range = false;
				_split_anchor_value = 0.0f;
			}
		}

		// 双击左键添加锚点
		if (canvasHovered && _track.nodes.size() < particles::kTodMaxTrackNodes && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hoveredNodeIndex < 0)
		{
			wx::Float32 time = 0.0f;
			wx::Float32 value = 0.0f;
			ScreenToTrack(graph, ImGui::GetIO().MousePos, _view_min, _view_max, time, value);
			_add_node(time, value);
		}

		ImGui::SetCursorScreenPos({ origin.x, canvas.max.y + 6.0f });
		const bool hasSegment = _selected_node_index >= 0 && _selected_node_index + 1 < static_cast<wx::Int32>(_track.nodes.size());
		ImGui::BeginDisabled(!hasSegment);
		wx::Int32 curveIndex = 0;
		if (hasSegment)
		{
			for (wx::Int32 index = 0; index < static_cast<wx::Int32>(sizeof(kCurveOptions) / sizeof(kCurveOptions[0])); ++index)
				if (kCurveOptions[index].type == _track.nodes[_selected_node_index].curve) curveIndex = index;
		}
		const wx::Int32 curveCount = static_cast<wx::Int32>(sizeof(kCurveOptions) / sizeof(kCurveOptions[0]));
		if (ImGui::ArrowButton("##previous_curve", ImGuiDir_Left) && curveIndex > 0)
			_track.nodes[_selected_node_index].curve = kCurveOptions[curveIndex - 1].type;
		_set_item_tooltip(ML("track.previous_curve"));
		ImGui::SameLine();
		const wx::Float32 arrowWidth = ImGui::GetFrameHeight();
		const wx::Float32 curveButtonWidth = canvasSize.x - arrowWidth * 2.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f;
		const char* curve_name = hasSegment
			? ML(kCurveOptions[curveIndex].name)
			: ML("track.select_segment");
		if (ImGui::Button(curve_name, { curveButtonWidth, 0.0f }) && hasSegment)
		{
			curveIndex = (curveIndex + 1) % curveCount;
			_track.nodes[_selected_node_index].curve = kCurveOptions[curveIndex].type;
		}
		_set_item_tooltip(ML("track.curve_to_next"));
		ImGui::SameLine();
		if (ImGui::ArrowButton("##next_curve", ImGuiDir_Right) && curveIndex + 1 < curveCount)
			_track.nodes[_selected_node_index].curve = kCurveOptions[curveIndex + 1].type;
		_set_item_tooltip(ML("track.next_curve"));
		ImGui::EndDisabled();
		ImGui::Dummy({ 0.0f, 4.0f });
	}

	void NodeTrackNode::_on_render_overlay()
	{
		Node::_on_render_overlay();
		if (_anchor_tooltip.empty())
			return;

		ImGui::BeginTooltip();
		ImGui::TextUnformatted(_anchor_tooltip.c_str());
		ImGui::EndTooltip();
	}

	ConstantNodeTrackNode::ConstantNodeTrackNode()
		: Node(),
		_value(0)
	{
		_name = "Constant";
		_serialization_type = "ConstantNodeTrack";
		_header_color = node_colors::header::ConstantNodeTrack;
	}

	ConstantNodeTrackNode::~ConstantNodeTrackNode()
	{

	}

	void ConstantNodeTrackNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "NodeTrack";
		output.name = "NodeTrack";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::NodeTrack;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> ConstantNodeTrackNode::_clone() const
	{
		auto node = std::make_unique<ConstantNodeTrackNode>();
		node->_value = _value;
		return node;
	}

	nlohmann::json ConstantNodeTrackNode::_serialize() const
	{
		return { { "value", _value } };
	}

	void ConstantNodeTrackNode::_deserialize(const nlohmann::json& data)
	{
		_value = data.value("value", 0.0f);
	}

	wx::Float32 ConstantNodeTrackNode::_get_value() const noexcept
	{
		return _value;
	}

	void ConstantNodeTrackNode::_set_value(wx::Float32 value) noexcept
	{
		_value = value;
	}

	void ConstantNodeTrackNode::_on_render_workspace()
	{
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputFloat("##float", &_value, 0.1f, 1.0f, "%.3f");
		ImGui::SameLine();
		ImGui::Dummy({ 10,0 });
	}
}
