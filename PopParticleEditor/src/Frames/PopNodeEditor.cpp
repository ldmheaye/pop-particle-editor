#include "../../include/Frames/PopNodeEditor.h"
#include "../../include/PopState.h"

#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace
{
	struct NodeImportData
	{
		wx::Size order;
		wx::SSize id;
		std::unique_ptr<pop::PopNodeEditor::Node> node;
		std::vector<wx::SSize> input_pin_ids;
		std::vector<wx::SSize> output_pin_ids;
	};

	struct LinkImportData
	{
		wx::SSize id;
		wx::SSize start_pin_id;
		wx::SSize end_pin_id;
		wx::Uint32 color;
	};
}

namespace pop
{
	void PopNodeEditor::_draw_pin_shape(const Pin& pin, bool connected)
	{
		PinShape shape = pin.shape;

		const ImVec2 size = { 18.0f, 18.0f };
		ImVec2 min = ImGui::GetCursorScreenPos();
		min.y += 6.0f;
		const ImVec2 max = { min.x + size.x, min.y + size.y };
		const ImVec2 center = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
		const ImU32 color = pin.color;
		auto* drawList = ImGui::GetWindowDrawList();

		ed::PinPivotRect(center, center);

		if (shape == PinShape::TriAngle)
		{
			const ImVec2 a = { min.x + 3.0f, min.y + 3.0f };
			const ImVec2 b = { max.x - 2.0f, center.y };
			const ImVec2 c = { min.x + 3.0f, max.y - 3.0f };
			if (connected)
				drawList->AddTriangleFilled(a, b, c, color);
			else
				drawList->AddTriangle(a, b, c, color, 2.0f);
		}
		else if (shape == PinShape::Square)
		{
			if (connected)
				drawList->AddRectFilled({ min.x + 3.0f, min.y + 3.0f }, { max.x - 3.0f, max.y - 3.0f }, color, 2.0f);
			else
				drawList->AddRect({ min.x + 3.0f, min.y + 3.0f }, { max.x - 3.0f, max.y - 3.0f }, color, 2.0f, 0, 2.0f);
		}
		else if (shape == PinShape::Circle)
		{
			if (connected)
				drawList->AddCircleFilled(center, 5.5f, color, 16);
			else
				drawList->AddCircle(center, 5.5f, color, 16, 2.0f);
		}

		ImGui::Dummy(size);
	}

	PopNodeEditor::PopNodeEditor()
		:_p_editor(nullptr),
		_context_node_id(0),
		_context_link_id(0),
		_next_id(0),
		_arrange_nodes_pending(false)
	{
		_create_editor();
	}

	void PopNodeEditor::_create_editor()
	{
		ed::Config config;
		config.SettingsFile = nullptr;
		config.SaveSettings = _save_editor_settings;
		config.LoadSettings = _load_editor_settings;
		config.UserPointer = this;
		_p_editor = ed::CreateEditor(&config);
	}

	PopNodeEditor::~PopNodeEditor()
	{
		if (_p_editor)
			ed::DestroyEditor(_p_editor);
	}

	bool PopNodeEditor::_save_editor_settings(
		const char* data,
		wx::Size size,
		ed::SaveReasonFlags,
		void* userPointer)
	{
		auto& editor = *static_cast<PopNodeEditor*>(userPointer);
		editor._editor_settings.assign(data, size);
		return true;
	}

	wx::Size PopNodeEditor::_load_editor_settings(char* data, void* userPointer)
	{
		const auto& settings =
			static_cast<PopNodeEditor*>(userPointer)->_editor_settings;
		if (data != nullptr && !settings.empty())
			std::memcpy(data, settings.data(), settings.size());
		return settings.size();
	}

	void PopNodeEditor::_reset_editor(std::string settings)
	{
		if (_p_editor != nullptr)
		{
			if (ed::GetCurrentEditor() == _p_editor)
				ed::SetCurrentEditor(nullptr);
			ed::DestroyEditor(_p_editor);
			_p_editor = nullptr;
		}

		_nodes.clear();
		_links.clear();
		_nodes_map.clear();
		_pins_map.clear();
		_context_node_id = 0;
		_context_link_id = 0;
		_next_id = 0;
		_arrange_nodes_pending = false;
		_editor_settings = std::move(settings);
		_create_editor();
	}

	void PopNodeEditor::Clear()
	{
		_reset_editor({});
	}

	void PopNodeEditor::OnRender()
	{
		ed::SetCurrentEditor(_p_editor);
		ed::EnableShortcuts(true);

		ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8.0f, 5.0f, 8.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f, 4.0f));

		ed::Begin("PopNodeEditor");

		for (auto& node : _nodes)
		{
			node->_on_render();
		}

		if (_arrange_nodes_pending)
		{
			_apply_node_layout();
			_arrange_nodes_pending = false;
			ed::NavigateToContent(0.0f);
		}

		for (auto& link : _links)
		{
			ed::Link(link.id, link.start_pin_id, link.end_pin_id, link.color, 2.0f);
		}

		_handle_link_creation();
		_handle_deletion();

		ed::NodeId nodeToCopy = 0;
		ed::NodeId nodeToDelete = 0;
		ed::LinkId linkToDelete = 0;

		ed::Suspend();
		if (ed::ShowNodeContextMenu(&_context_node_id))
			ImGui::OpenPopup("Node Context Menu");
		else if (ed::ShowLinkContextMenu(&_context_link_id))
			ImGui::OpenPopup("Link Context Menu");

		if (ImGui::BeginPopup("Node Context Menu"))
		{
			const auto nodeItr = _nodes_map.find(
				reinterpret_cast<wx::Size>(_context_node_id.AsPointer()));
			const bool nodeExists = nodeItr != _nodes_map.end();

			ImGui::BeginDisabled(!nodeExists);
			if (ImGui::MenuItem(ML("node.copy")))
				nodeToCopy = _context_node_id;
			if (ImGui::MenuItem(ML("node.delete")))
				nodeToDelete = _context_node_id;
			ImGui::EndDisabled();

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("Link Context Menu"))
		{
			if (ImGui::MenuItem(ML("link.delete")))
				linkToDelete = _context_link_id;

			ImGui::EndPopup();
		}
		ed::Resume();

		if (nodeToCopy.AsPointer() != nullptr)
		{
			const auto nodeItr = _nodes_map.find(
				reinterpret_cast<wx::Size>(nodeToCopy.AsPointer()));
			if (nodeItr != _nodes_map.end())
			{
				const ImVec2 sourcePosition = ed::GetNodePosition(nodeToCopy);
				auto copiedNode = nodeItr->second->_clone();
				Node* copiedNodePtr = copiedNode.get();
				AddNode(std::move(copiedNode), false);
				ed::SetNodePosition(
					copiedNodePtr->_get_id(),
					{ sourcePosition.x + 30.0f, sourcePosition.y + 30.0f });
			}
		}

		if (linkToDelete.AsPointer() != nullptr)
			ed::DeleteLink(linkToDelete);
		if (nodeToDelete.AsPointer() != nullptr)
			ed::DeleteNode(nodeToDelete);

		ed::End();

		ed::PopStyleVar();
		ImGui::PopStyleVar(3);
		ed::SetCurrentEditor(nullptr);

		for (auto& node : _nodes)
		{
			node->_on_render_overlay();
		}
	}

	wx::Size PopNodeEditor::_get_next_id()
	{
		_next_id++;
		return _next_id;
	}

	void PopNodeEditor::_arrange_nodes()
	{
		_arrange_nodes_pending = true;
	}

	void PopNodeEditor::_apply_node_layout()
	{
		if (_nodes.empty())
			return;

		constexpr wx::Float32 layerSpacing = 160.0f;
		constexpr wx::Float32 verticalSpacing = 28.0f;
		constexpr wx::Float32 defaultNodeWidth = 180.0f;
		constexpr wx::Float32 defaultNodeHeight = 80.0f;
		constexpr wx::Size orderingPassCount = 4;

		const wx::Size nodeCount = _nodes.size();
		std::unordered_map<wx::Size, wx::Size> nodeIndices;
		nodeIndices.reserve(nodeCount);
		std::vector<ImVec2> nodeSizes(nodeCount);
		std::vector<std::vector<wx::Size>> predecessors(nodeCount);
		std::vector<std::vector<wx::Size>> successors(nodeCount);
		std::vector<std::vector<const Pin*>> outgoingSourcePins(nodeCount);
		std::vector<std::vector<const Pin*>> outgoingTargetPins(nodeCount);
		std::vector<std::vector<const Pin*>> incomingSourcePins(nodeCount);
		std::vector<std::vector<const Pin*>> incomingTargetPins(nodeCount);
		std::vector<wx::Size> remainingOutgoing(nodeCount, 0);
		std::vector<wx::Size> distanceToSink(nodeCount, 0);

		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			const Node& node = *_nodes[nodeIndex];
			const wx::Size nodeId = reinterpret_cast<wx::Size>(
				node._id.AsPointer());
			nodeIndices[nodeId] = nodeIndex;

			const ImVec2 size = ed::GetNodeSize(node._id);
			nodeSizes[nodeIndex] = {
				size.x > 0.0f ? size.x : defaultNodeWidth,
				size.y > 0.0f ? size.y : defaultNodeHeight
			};
		}

		for (const Link& link : _links)
		{
			const auto firstPinItr = _pins_map.find(
				reinterpret_cast<wx::Size>(link.start_pin_id.AsPointer()));
			const auto secondPinItr = _pins_map.find(
				reinterpret_cast<wx::Size>(link.end_pin_id.AsPointer()));
			if (firstPinItr == _pins_map.end() ||
				secondPinItr == _pins_map.end() ||
				firstPinItr->second == nullptr || secondPinItr->second == nullptr)
			{
				continue;
			}

			const Pin* outputPin = firstPinItr->second->kind == ed::PinKind::Output
				? firstPinItr->second
				: secondPinItr->second;
			const Pin* inputPin = firstPinItr->second->kind == ed::PinKind::Input
				? firstPinItr->second
				: secondPinItr->second;
			if (outputPin->kind != ed::PinKind::Output ||
				inputPin->kind != ed::PinKind::Input)
			{
				continue;
			}

			const auto sourceItr = nodeIndices.find(
				reinterpret_cast<wx::Size>(outputPin->node_id.AsPointer()));
			const auto targetItr = nodeIndices.find(
				reinterpret_cast<wx::Size>(inputPin->node_id.AsPointer()));
			if (sourceItr == nodeIndices.end() || targetItr == nodeIndices.end() ||
				sourceItr->second == targetItr->second)
			{
				continue;
			}

			++remainingOutgoing[sourceItr->second];
			predecessors[targetItr->second].push_back(sourceItr->second);
			successors[sourceItr->second].push_back(targetItr->second);
			outgoingSourcePins[sourceItr->second].push_back(outputPin);
			outgoingTargetPins[sourceItr->second].push_back(inputPin);
			incomingSourcePins[targetItr->second].push_back(outputPin);
			incomingTargetPins[targetItr->second].push_back(inputPin);
		}

		std::vector<wx::Size> readyNodes;
		readyNodes.reserve(nodeCount);
		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
			if (remainingOutgoing[nodeIndex] == 0)
				readyNodes.push_back(nodeIndex);

		for (wx::Size readyIndex = 0; readyIndex < readyNodes.size(); ++readyIndex)
		{
			const wx::Size nodeIndex = readyNodes[readyIndex];
			for (const wx::Size predecessorIndex : predecessors[nodeIndex])
			{
				distanceToSink[predecessorIndex] = std::max(
					distanceToSink[predecessorIndex],
					distanceToSink[nodeIndex] + 1);
				if (remainingOutgoing[predecessorIndex] > 0 &&
					--remainingOutgoing[predecessorIndex] == 0)
				{
					readyNodes.push_back(predecessorIndex);
				}
			}
		}

		const wx::Size maxDistance = *std::max_element(
			distanceToSink.begin(),
			distanceToSink.end());
		std::vector<std::vector<wx::Size>> layers(maxDistance + 1);
		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			const wx::Size layerIndex = maxDistance - distanceToSink[nodeIndex];
			layers[layerIndex].push_back(nodeIndex);
		}

		auto getPinOffset = [this, &nodeSizes](
			const Pin* pin,
			wx::Size nodeIndex)
		{
			if (pin != nullptr && pin->layout_y >= 0.0f)
				return pin->layout_y;

			if (pin == nullptr)
				return nodeSizes[nodeIndex].y * 0.5f;

			const auto& pins = pin->kind == ed::PinKind::Input
				? _nodes[nodeIndex]->_input_pins
				: _nodes[nodeIndex]->_output_pins;
			wx::Size pinIndex = 0;
			for (const Pin& candidate : pins)
			{
				if (candidate.id == pin->id)
					break;
				++pinIndex;
			}
			return nodeSizes[nodeIndex].y *
				static_cast<wx::Float32>(pinIndex + 1) /
				static_cast<wx::Float32>(pins.size() + 1);
		};
		auto getInputPinOrder = [this](
			const Pin* pin,
			wx::Size nodeIndex)
		{
			wx::Size pinOrder = 0;
			if (pin == nullptr || pin->kind != ed::PinKind::Input)
				return std::numeric_limits<wx::Size>::max();

			for (const Pin& inputPin : _nodes[nodeIndex]->_input_pins)
			{
				if (inputPin.id == pin->id)
					return pinOrder;
				++pinOrder;
			}
			return std::numeric_limits<wx::Size>::max();
		};
		std::vector<std::vector<wx::Size>> treeOrderPaths(nodeCount);
		std::vector<wx::Uint8> hasTreeOrderPath(nodeCount, 0);
		wx::Size rootOrder = 0;
		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			if (!successors[nodeIndex].empty())
				continue;

			treeOrderPaths[nodeIndex].push_back(rootOrder++);
			hasTreeOrderPath[nodeIndex] = 1;
		}

		for (const wx::Size targetIndex : readyNodes)
		{
			if (hasTreeOrderPath[targetIndex] == 0)
			{
				treeOrderPaths[targetIndex].push_back(rootOrder++);
				hasTreeOrderPath[targetIndex] = 1;
			}

			std::vector<wx::Size> edgeOrders;
			edgeOrders.reserve(predecessors[targetIndex].size());
			for (wx::Size edgeIndex = 0;
				edgeIndex < predecessors[targetIndex].size();
				++edgeIndex)
			{
				edgeOrders.push_back(edgeIndex);
			}
			std::stable_sort(
				edgeOrders.begin(),
				edgeOrders.end(),
				[&](wx::Size left, wx::Size right)
				{
					return getInputPinOrder(
						incomingTargetPins[targetIndex][left],
						targetIndex) < getInputPinOrder(
						incomingTargetPins[targetIndex][right],
						targetIndex);
				});

			wx::Size previousPinOrder = std::numeric_limits<wx::Size>::max();
			wx::Size connectionOrder = 0;
			for (const wx::Size edgeIndex : edgeOrders)
			{
				const wx::Size pinOrder = getInputPinOrder(
					incomingTargetPins[targetIndex][edgeIndex],
					targetIndex);
				if (pinOrder == previousPinOrder)
				{
					++connectionOrder;
				}
				else
				{
					previousPinOrder = pinOrder;
					connectionOrder = 0;
				}

				const wx::Size sourceIndex = predecessors[targetIndex][edgeIndex];
				std::vector<wx::Size> candidatePath = treeOrderPaths[targetIndex];
				candidatePath.push_back(pinOrder);
				candidatePath.push_back(connectionOrder);
				if (hasTreeOrderPath[sourceIndex] == 0 ||
					std::lexicographical_compare(
						candidatePath.begin(),
						candidatePath.end(),
						treeOrderPaths[sourceIndex].begin(),
						treeOrderPaths[sourceIndex].end()))
				{
					treeOrderPaths[sourceIndex] = std::move(candidatePath);
					hasTreeOrderPath[sourceIndex] = 1;
				}
			}
		}

		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			if (hasTreeOrderPath[nodeIndex] != 0)
				continue;

			treeOrderPaths[nodeIndex].push_back(rootOrder++);
			hasTreeOrderPath[nodeIndex] = 1;
		}

		std::vector<wx::Float32> orderingY(nodeCount, 0.0f);
		std::vector<wx::Float32> orderingScore(nodeCount, 0.0f);
		std::vector<wx::Uint8> hasOrderingScore(nodeCount, 0);
		auto updateOrderingY = [&]()
		{
			for (const auto& layer : layers)
			{
				wx::Float32 y = 0.0f;
				for (const wx::Size nodeIndex : layer)
				{
					orderingY[nodeIndex] = y;
					y += nodeSizes[nodeIndex].y + verticalSpacing;
				}
			}
		};
		auto sortLayer = [&](wx::Size layerIndex)
		{
			std::stable_sort(
				layers[layerIndex].begin(),
				layers[layerIndex].end(),
				[&](wx::Size left, wx::Size right)
				{
					if (treeOrderPaths[left] != treeOrderPaths[right])
					{
						return std::lexicographical_compare(
							treeOrderPaths[left].begin(),
							treeOrderPaths[left].end(),
							treeOrderPaths[right].begin(),
							treeOrderPaths[right].end());
					}
					if (hasOrderingScore[left] != hasOrderingScore[right])
						return hasOrderingScore[left] > hasOrderingScore[right];
					if (hasOrderingScore[left] == 0)
						return false;
					return orderingScore[left] < orderingScore[right];
				});
		};

		for (wx::Size pass = 0; pass < orderingPassCount; ++pass)
		{
			for (wx::Size reverseLayer = layers.size(); reverseLayer > 1;
				--reverseLayer)
			{
				updateOrderingY();
				const wx::Size layerIndex = reverseLayer - 2;
				for (const wx::Size nodeIndex : layers[layerIndex])
				{
					orderingScore[nodeIndex] = 0.0f;
					hasOrderingScore[nodeIndex] = successors[nodeIndex].empty()
						? 0
						: 1;
					for (wx::Size edgeIndex = 0;
						edgeIndex < successors[nodeIndex].size();
						++edgeIndex)
					{
						const wx::Size targetIndex =
							successors[nodeIndex][edgeIndex];
						orderingScore[nodeIndex] += orderingY[targetIndex] +
							getPinOffset(
								outgoingTargetPins[nodeIndex][edgeIndex],
								targetIndex) -
							getPinOffset(
								outgoingSourcePins[nodeIndex][edgeIndex],
								nodeIndex);
					}
					if (hasOrderingScore[nodeIndex] != 0)
					{
						orderingScore[nodeIndex] /= static_cast<wx::Float32>(
							successors[nodeIndex].size());
					}
				}
				sortLayer(layerIndex);
			}

			for (wx::Size layerIndex = 1; layerIndex < layers.size();
				++layerIndex)
			{
				updateOrderingY();
				for (const wx::Size nodeIndex : layers[layerIndex])
				{
					orderingScore[nodeIndex] = 0.0f;
					hasOrderingScore[nodeIndex] = predecessors[nodeIndex].empty()
						? 0
						: 1;
					for (wx::Size edgeIndex = 0;
						edgeIndex < predecessors[nodeIndex].size();
						++edgeIndex)
					{
						const wx::Size sourceIndex =
							predecessors[nodeIndex][edgeIndex];
						orderingScore[nodeIndex] += orderingY[sourceIndex] +
							getPinOffset(
								incomingSourcePins[nodeIndex][edgeIndex],
								sourceIndex) -
							getPinOffset(
								incomingTargetPins[nodeIndex][edgeIndex],
								nodeIndex);
					}
					if (hasOrderingScore[nodeIndex] != 0)
					{
						orderingScore[nodeIndex] /= static_cast<wx::Float32>(
							predecessors[nodeIndex].size());
					}
				}
				sortLayer(layerIndex);
			}
		}

		std::vector<wx::Float32> layerWidths(layers.size(), 0.0f);
		std::vector<wx::Float32> layerX(layers.size(), 0.0f);
		wx::Float32 x = 0.0f;
		for (wx::Size layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
		{
			layerX[layerIndex] = x;
			for (const wx::Size nodeIndex : layers[layerIndex])
			{
				layerWidths[layerIndex] = std::max(
					layerWidths[layerIndex],
					nodeSizes[nodeIndex].x);
			}
			x += layerWidths[layerIndex] + layerSpacing;
		}

		std::vector<wx::Float32> nodeY(nodeCount, 0.0f);
		std::vector<wx::Float32> desiredY(nodeCount, 0.0f);
		std::vector<wx::Uint8> hasDesiredY(nodeCount, 0);
		for (wx::Size reverseLayer = layers.size(); reverseLayer > 0;
			--reverseLayer)
		{
			const wx::Size layerIndex = reverseLayer - 1;
			for (const wx::Size nodeIndex : layers[layerIndex])
			{
				desiredY[nodeIndex] = 0.0f;
				hasDesiredY[nodeIndex] = successors[nodeIndex].empty() ? 0 : 1;
				for (wx::Size edgeIndex = 0;
					edgeIndex < successors[nodeIndex].size();
					++edgeIndex)
				{
					const wx::Size targetIndex =
						successors[nodeIndex][edgeIndex];
					desiredY[nodeIndex] += nodeY[targetIndex] +
						getPinOffset(
							outgoingTargetPins[nodeIndex][edgeIndex],
							targetIndex) -
						getPinOffset(
							outgoingSourcePins[nodeIndex][edgeIndex],
							nodeIndex);
				}
				if (hasDesiredY[nodeIndex] != 0)
				{
					desiredY[nodeIndex] /= static_cast<wx::Float32>(
						successors[nodeIndex].size());
				}
			}

			std::stable_sort(
				layers[layerIndex].begin(),
				layers[layerIndex].end(),
				[&](wx::Size left, wx::Size right)
				{
					if (treeOrderPaths[left] != treeOrderPaths[right])
					{
						return std::lexicographical_compare(
							treeOrderPaths[left].begin(),
							treeOrderPaths[left].end(),
							treeOrderPaths[right].begin(),
							treeOrderPaths[right].end());
					}
					if (hasDesiredY[left] != hasDesiredY[right])
						return hasDesiredY[left] > hasDesiredY[right];
					if (hasDesiredY[left] == 0)
						return false;
					return desiredY[left] < desiredY[right];
				});

			wx::Float32 previousBottom = 0.0f;
			bool hasPreviousNode = false;
			wx::Float32 shiftTotal = 0.0f;
			wx::Size shiftCount = 0;
			for (const wx::Size nodeIndex : layers[layerIndex])
			{
				const wx::Float32 minimumY = hasPreviousNode
					? previousBottom + verticalSpacing
					: 0.0f;
				nodeY[nodeIndex] = hasDesiredY[nodeIndex] != 0
					? std::max(desiredY[nodeIndex], minimumY)
					: minimumY;
				if (hasDesiredY[nodeIndex] != 0)
				{
					shiftTotal += desiredY[nodeIndex] - nodeY[nodeIndex];
					++shiftCount;
				}
				previousBottom = nodeY[nodeIndex] + nodeSizes[nodeIndex].y;
				hasPreviousNode = true;
			}

			if (shiftCount > 0)
			{
				const wx::Float32 shift = shiftTotal /
					static_cast<wx::Float32>(shiftCount);
				for (const wx::Size nodeIndex : layers[layerIndex])
					nodeY[nodeIndex] += shift;
			}
		}

		std::vector<wx::Float32> nodeX(nodeCount, 0.0f);
		for (wx::Size layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
		{
			for (const wx::Size nodeIndex : layers[layerIndex])
			{
				nodeX[nodeIndex] = layerX[layerIndex] +
					(layerWidths[layerIndex] - nodeSizes[nodeIndex].x) * 0.5f;
			}
		}

		wx::Size primarySink = nodeCount;
		wx::Size primarySinkInputCount = 0;
		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			if (!successors[nodeIndex].empty() ||
				predecessors[nodeIndex].size() <= primarySinkInputCount)
			{
				continue;
			}
			primarySink = nodeIndex;
			primarySinkInputCount = predecessors[nodeIndex].size();
		}

		std::vector<wx::Size> branchRoots;
		if (primarySink < nodeCount)
		{
			for (const wx::Size predecessorIndex : predecessors[primarySink])
			{
				if (std::find(
					branchRoots.begin(),
					branchRoots.end(),
					predecessorIndex) == branchRoots.end())
				{
					branchRoots.push_back(predecessorIndex);
				}
			}
			std::stable_sort(
				branchRoots.begin(),
				branchRoots.end(),
				[&](wx::Size left, wx::Size right)
				{
					return std::lexicographical_compare(
						treeOrderPaths[left].begin(),
						treeOrderPaths[left].end(),
						treeOrderPaths[right].begin(),
						treeOrderPaths[right].end());
				});
		}

		if (!branchRoots.empty())
		{
			constexpr wx::Float32 branchHorizontalSpacing = 120.0f;
			constexpr wx::Float32 branchVerticalSpacing = 80.0f;
			constexpr wx::Float32 sinkSpacing = 220.0f;

			const wx::Size branchCount = branchRoots.size();
			const wx::Size invalidBranch = std::numeric_limits<wx::Size>::max();
			std::vector<wx::Size> branchOwners(nodeCount, invalidBranch);
			std::vector<std::vector<wx::Size>> branchNodes(branchCount);
			for (wx::Size branchIndex = 0; branchIndex < branchCount;
				++branchIndex)
			{
				std::vector<wx::Size> pendingNodes{ branchRoots[branchIndex] };
				while (!pendingNodes.empty())
				{
					const wx::Size nodeIndex = pendingNodes.back();
					pendingNodes.pop_back();
					if (nodeIndex == primarySink ||
						branchOwners[nodeIndex] != invalidBranch)
					{
						continue;
					}

					branchOwners[nodeIndex] = branchIndex;
					branchNodes[branchIndex].push_back(nodeIndex);
					for (const wx::Size predecessorIndex : predecessors[nodeIndex])
						pendingNodes.push_back(predecessorIndex);
				}
			}

			for (wx::Size branchIndex = 0; branchIndex < branchCount;
				++branchIndex)
			{
				for (wx::Size reverseLayer = layers.size(); reverseLayer > 0;
					--reverseLayer)
				{
					const wx::Size layerIndex = reverseLayer - 1;
					std::vector<wx::Size> localLayerNodes;
					for (const wx::Size nodeIndex : layers[layerIndex])
					{
						if (branchOwners[nodeIndex] == branchIndex)
							localLayerNodes.push_back(nodeIndex);
					}
					if (localLayerNodes.empty())
						continue;

					for (const wx::Size nodeIndex : localLayerNodes)
					{
						desiredY[nodeIndex] = 0.0f;
						hasDesiredY[nodeIndex] = nodeIndex == branchRoots[branchIndex]
							? 1
							: 0;
						wx::Size connectedSuccessorCount = 0;
						for (wx::Size edgeIndex = 0;
							edgeIndex < successors[nodeIndex].size();
							++edgeIndex)
						{
							const wx::Size targetIndex =
								successors[nodeIndex][edgeIndex];
							if (branchOwners[targetIndex] != branchIndex)
								continue;

							desiredY[nodeIndex] += nodeY[targetIndex] +
								getPinOffset(
									outgoingTargetPins[nodeIndex][edgeIndex],
									targetIndex) -
								getPinOffset(
									outgoingSourcePins[nodeIndex][edgeIndex],
									nodeIndex);
							++connectedSuccessorCount;
						}
						if (connectedSuccessorCount > 0)
						{
							desiredY[nodeIndex] /= static_cast<wx::Float32>(
								connectedSuccessorCount);
							hasDesiredY[nodeIndex] = 1;
						}
					}

					std::stable_sort(
						localLayerNodes.begin(),
						localLayerNodes.end(),
						[&](wx::Size left, wx::Size right)
						{
							if (treeOrderPaths[left] != treeOrderPaths[right])
							{
								return std::lexicographical_compare(
									treeOrderPaths[left].begin(),
									treeOrderPaths[left].end(),
									treeOrderPaths[right].begin(),
									treeOrderPaths[right].end());
							}
							if (hasDesiredY[left] != hasDesiredY[right])
								return hasDesiredY[left] > hasDesiredY[right];
							if (hasDesiredY[left] == 0)
								return false;
							return desiredY[left] < desiredY[right];
						});

					wx::Float32 previousBottom = 0.0f;
					bool hasPreviousNode = false;
					wx::Float32 shiftTotal = 0.0f;
					wx::Size shiftCount = 0;
					for (const wx::Size nodeIndex : localLayerNodes)
					{
						const wx::Float32 minimumY = hasPreviousNode
							? previousBottom + verticalSpacing
							: desiredY[nodeIndex];
						nodeY[nodeIndex] = hasDesiredY[nodeIndex] != 0
							? std::max(desiredY[nodeIndex], minimumY)
							: minimumY;
						if (hasDesiredY[nodeIndex] != 0)
						{
							shiftTotal += desiredY[nodeIndex] - nodeY[nodeIndex];
							++shiftCount;
						}
						previousBottom = nodeY[nodeIndex] + nodeSizes[nodeIndex].y;
						hasPreviousNode = true;
					}
					if (shiftCount > 0)
					{
						const wx::Float32 shift = shiftTotal /
							static_cast<wx::Float32>(shiftCount);
						for (const wx::Size nodeIndex : localLayerNodes)
							nodeY[nodeIndex] += shift;
					}
				}
			}

			std::vector<wx::Float32> branchMinimumX(
				branchCount,
				std::numeric_limits<wx::Float32>::max());
			std::vector<wx::Float32> branchMinimumY(
				branchCount,
				std::numeric_limits<wx::Float32>::max());
			std::vector<wx::Float32> branchMaximumX(
				branchCount,
				std::numeric_limits<wx::Float32>::lowest());
			std::vector<wx::Float32> branchMaximumY(
				branchCount,
				std::numeric_limits<wx::Float32>::lowest());
			std::vector<wx::Float32> branchWidths(branchCount, 0.0f);
			std::vector<wx::Float32> branchHeights(branchCount, 0.0f);
			for (wx::Size branchIndex = 0; branchIndex < branchCount;
				++branchIndex)
			{
				for (const wx::Size nodeIndex : branchNodes[branchIndex])
				{
					branchMinimumX[branchIndex] = std::min(
						branchMinimumX[branchIndex],
						nodeX[nodeIndex]);
					branchMinimumY[branchIndex] = std::min(
						branchMinimumY[branchIndex],
						nodeY[nodeIndex]);
					branchMaximumX[branchIndex] = std::max(
						branchMaximumX[branchIndex],
						nodeX[nodeIndex] + nodeSizes[nodeIndex].x);
					branchMaximumY[branchIndex] = std::max(
						branchMaximumY[branchIndex],
						nodeY[nodeIndex] + nodeSizes[nodeIndex].y);
				}
				branchWidths[branchIndex] =
					branchMaximumX[branchIndex] - branchMinimumX[branchIndex];
				branchHeights[branchIndex] =
					branchMaximumY[branchIndex] - branchMinimumY[branchIndex];
			}

			wx::Size selectedColumnCount = 1;
			wx::Float32 bestBalanceScore =
				std::numeric_limits<wx::Float32>::max();
			for (wx::Size candidateColumnCount = 1;
				candidateColumnCount <= branchCount;
				++candidateColumnCount)
			{
				const wx::Size rowCount =
					(branchCount + candidateColumnCount - 1) /
					candidateColumnCount;
				std::vector<wx::Float32> columnWidths(
					candidateColumnCount,
					0.0f);
				std::vector<wx::Float32> rowHeights(rowCount, 0.0f);
				for (wx::Size branchIndex = 0; branchIndex < branchCount;
					++branchIndex)
				{
					const wx::Size columnIndex =
						branchIndex % candidateColumnCount;
					const wx::Size rowIndex =
						branchIndex / candidateColumnCount;
					columnWidths[columnIndex] = std::max(
						columnWidths[columnIndex],
						branchWidths[branchIndex]);
					rowHeights[rowIndex] = std::max(
						rowHeights[rowIndex],
						branchHeights[branchIndex]);
				}

				wx::Float32 totalWidth = sinkSpacing +
					nodeSizes[primarySink].x;
				for (const wx::Float32 columnWidth : columnWidths)
					totalWidth += columnWidth;
				if (candidateColumnCount > 1)
				{
					totalWidth += branchHorizontalSpacing *
						static_cast<wx::Float32>(candidateColumnCount - 1);
				}

				wx::Float32 totalHeight = 0.0f;
				for (const wx::Float32 rowHeight : rowHeights)
					totalHeight += rowHeight;
				if (rowCount > 1)
				{
					totalHeight += branchVerticalSpacing *
						static_cast<wx::Float32>(rowCount - 1);
				}

				const wx::Float32 largerSize = std::max(totalWidth, totalHeight);
				const wx::Float32 balanceScore = largerSize > 0.0f
					? (largerSize - std::min(totalWidth, totalHeight)) / largerSize
					: 0.0f;
				if (balanceScore < bestBalanceScore)
				{
					bestBalanceScore = balanceScore;
					selectedColumnCount = candidateColumnCount;
				}
			}

			const wx::Size selectedRowCount =
				(branchCount + selectedColumnCount - 1) /
				selectedColumnCount;
			std::vector<wx::Float32> columnWidths(selectedColumnCount, 0.0f);
			std::vector<wx::Float32> rowHeights(selectedRowCount, 0.0f);
			for (wx::Size branchIndex = 0; branchIndex < branchCount;
				++branchIndex)
			{
				const wx::Size columnIndex = branchIndex % selectedColumnCount;
				const wx::Size rowIndex = branchIndex / selectedColumnCount;
				columnWidths[columnIndex] = std::max(
					columnWidths[columnIndex],
					branchWidths[branchIndex]);
				rowHeights[rowIndex] = std::max(
					rowHeights[rowIndex],
					branchHeights[branchIndex]);
			}

			std::vector<wx::Float32> columnX(selectedColumnCount, 0.0f);
			std::vector<wx::Float32> rowY(selectedRowCount, 0.0f);
			for (wx::Size columnIndex = 1; columnIndex < selectedColumnCount;
				++columnIndex)
			{
				columnX[columnIndex] = columnX[columnIndex - 1] +
					columnWidths[columnIndex - 1] + branchHorizontalSpacing;
			}
			for (wx::Size rowIndex = 1; rowIndex < selectedRowCount; ++rowIndex)
			{
				rowY[rowIndex] = rowY[rowIndex - 1] +
					rowHeights[rowIndex - 1] + branchVerticalSpacing;
			}

			for (wx::Size branchIndex = 0; branchIndex < branchCount;
				++branchIndex)
			{
				const wx::Size columnIndex = branchIndex % selectedColumnCount;
				const wx::Size rowIndex = branchIndex / selectedColumnCount;
				const wx::Float32 branchX = columnX[columnIndex] +
					(columnWidths[columnIndex] - branchWidths[branchIndex]) * 0.5f;
				const wx::Float32 branchY = rowY[rowIndex] +
					(rowHeights[rowIndex] - branchHeights[branchIndex]) * 0.5f;
				for (const wx::Size nodeIndex : branchNodes[branchIndex])
				{
					nodeX[nodeIndex] = branchX + nodeX[nodeIndex] -
						branchMinimumX[branchIndex];
					nodeY[nodeIndex] = branchY + nodeY[nodeIndex] -
						branchMinimumY[branchIndex];
				}
			}

			const wx::Float32 gridWidth = columnX.back() + columnWidths.back();
			const wx::Float32 gridHeight = rowY.back() + rowHeights.back();
			nodeX[primarySink] = gridWidth + sinkSpacing;
			nodeY[primarySink] = std::max(
				(gridHeight - nodeSizes[primarySink].y) * 0.5f,
				0.0f);

			wx::Float32 detachedY = gridHeight + branchVerticalSpacing;
			for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
			{
				if (nodeIndex == primarySink ||
					branchOwners[nodeIndex] != invalidBranch)
				{
					continue;
				}
				nodeX[nodeIndex] = 0.0f;
				nodeY[nodeIndex] = detachedY;
				detachedY += nodeSizes[nodeIndex].y + verticalSpacing;
			}
		}
		else
		{
			wx::Float32 minimumY = std::numeric_limits<wx::Float32>::max();
			for (const wx::Float32 y : nodeY)
				minimumY = std::min(minimumY, y);
			for (wx::Float32& y : nodeY)
				y -= minimumY;
		}

		for (wx::Size nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			ed::SetNodePosition(
				_nodes[nodeIndex]->_id,
				{ nodeX[nodeIndex], nodeY[nodeIndex] });
		}
	}

	void PopNodeEditor::AddNode(std::unique_ptr<Node>&& node, bool moveToMouse)
	{
		node->_id = _get_next_id();
		_nodes_map[reinterpret_cast<wx::Size>(node->_id.AsPointer())] = node.get();

		auto* buf = ed::GetCurrentEditor();

		ed::SetCurrentEditor(_p_editor);

		if (moveToMouse)
		{
			ed::SetNodePosition(node->_id, ed::ScreenToCanvas(ImGui::GetMousePos()));
		}
		else
		{
			ed::SetNodePosition(node->_id, { 0,0 });
		}

		ed::SetCurrentEditor(buf);

		_nodes.emplace_back(std::move(node));

		_nodes.back()->_on_initialize(*this);
	}

	PopNodeEditor::Node* PopNodeEditor::_find_node(std::string_view name, const Node* excluded)
	{
		for (const auto& node : _nodes)
		{
			if (node.get() != excluded &&
				std::string_view(node->_name.data(), node->_name.size()) == name)
				return node.get();
		}

		return nullptr;
	}

	PopNodeEditor::Pin& PopNodeEditor::_add_node_pin(Node& node, const Pin& pin)
	{
		auto pinId = _get_next_id();
		_pins_map[pinId] = &node._store_pin(pin, pinId);
		return *_pins_map.at(pinId);
	}

	void PopNodeEditor::_add_link(Pin& start, Pin& end)
	{
		Link link { _get_next_id(), start.id, end.id, start.color };

		_links.push_back(link);

		start.connected_links.push_back(&_links.back());
		end.connected_links.push_back(&_links.back());

	}

	void PopNodeEditor::_delete_link(ax::NodeEditor::LinkId id)
	{
		for (auto itr = _links.begin(); itr != _links.end(); itr++)
		{
			if (itr->id == id)
			{
				const auto startItr = _pins_map.find(
					reinterpret_cast<wx::Size>(itr->start_pin_id.AsPointer()));
				const auto endItr = _pins_map.find(
					reinterpret_cast<wx::Size>(itr->end_pin_id.AsPointer()));
				if (startItr != _pins_map.end() && startItr->second != nullptr)
				{
					auto& connectedLinks = startItr->second->connected_links;
					connectedLinks.erase(std::remove(
						connectedLinks.begin(),
						connectedLinks.end(),
						&(*itr)), connectedLinks.end());
				}
				if (endItr != _pins_map.end() && endItr->second != nullptr)
				{
					auto& connectedLinks = endItr->second->connected_links;
					connectedLinks.erase(std::remove(
						connectedLinks.begin(),
						connectedLinks.end(),
						&(*itr)), connectedLinks.end());
				}

				_links.erase(itr);
				return;
			}
		}
	}

	void PopNodeEditor::_delete_node(ax::NodeEditor::NodeId id)
	{
		for (wx::Size i = 0; i < _nodes.size(); i++)
		{
			if (_nodes[i]->_id.AsPointer() == id.AsPointer())
			{
				for (const auto& pin : _nodes[i]->_input_pins)
				{
					auto itr = _pins_map.find(reinterpret_cast<wx::Size>(pin.id.AsPointer()));
					if (itr == _pins_map.end() || itr->second == nullptr)
						continue;
					while (itr->second->connected_links.size())
					{
						_delete_link(itr->second->connected_links[0]->id);
					}

					_pins_map.erase(itr);
				}

				for (const auto& pin : _nodes[i]->_output_pins)
				{
					auto itr = _pins_map.find(reinterpret_cast<wx::Size>(pin.id.AsPointer()));
					if (itr == _pins_map.end() || itr->second == nullptr)
						continue;

					while (itr->second->connected_links.size())
					{
						_delete_link(itr->second->connected_links[0]->id);
					}

					_pins_map.erase(itr);
				}

				_nodes_map.erase(reinterpret_cast<wx::Size>(id.AsPointer()));
				_nodes.erase(_nodes.begin() + i);
				return;
			}
		}
	}

	void PopNodeEditor::_handle_link_creation()
	{
		if (ed::BeginCreate())
		{
			ed::PinId firstId = 0;
			ed::PinId secondId = 0;

			if (ed::QueryNewLink(&firstId, &secondId))
			{
				auto* first = _pins_map[reinterpret_cast<wx::Size>(firstId.AsPointer())];
				auto* second = _pins_map[reinterpret_cast<wx::Size>(secondId.AsPointer())];

				if (Pin::_can_create_link(*first, *second))
				{
					const ImColor color = first->color;

					if (ed::AcceptNewItem(color, 2.0f))
						_add_link(*first, *second);
				}
				else
				{
					ed::RejectNewItem(ImColor(220, 72, 72), 2.0f);
				}
			}
		}
		ed::EndCreate();
	}

	void PopNodeEditor::_handle_deletion()
	{
		if (!ed::BeginDelete())
			return;

		std::vector<ed::LinkId> linksToDelete;
		std::vector<ed::NodeId> nodesToDelete;
		ed::LinkId linkId = 0;
		while (ed::QueryDeletedLink(&linkId))
			if (ed::AcceptDeletedItem())
				linksToDelete.push_back(linkId);

		ed::NodeId nodeId = 0;
		while (ed::QueryDeletedNode(&nodeId))
			if (ed::AcceptDeletedItem())
				nodesToDelete.push_back(nodeId);

		ed::EndDelete();

		for (const ed::LinkId acceptedLinkId : linksToDelete)
			_delete_link(acceptedLinkId);
		for (const ed::NodeId acceptedNodeId : nodesToDelete)
			_delete_node(acceptedNodeId);
	}

	nlohmann::json PopNodeEditor::Export() const
	{
		nlohmann::json settings = nlohmann::json::object();
		if (!_editor_settings.empty())
		{
			settings = nlohmann::json::parse(
				_editor_settings,
				nullptr,
				false);
			if (settings.is_discarded())
				settings = nlohmann::json::object();
		}

		nlohmann::json document = {
			{ "format", "PopParticleEditor" },
			{ "version", 1 },
			{ "node_editor", {
				{ "settings_file", std::move(settings) },
				{ "next_id", _next_id }
			} },
			{ "nodes", nlohmann::json::object() },
			{ "links", nlohmann::json::object() }
		};

		wx::Size order = 0;
		for (const auto& node : _nodes)
		{
			auto serializePinIds = [](const std::list<Pin>& pins)
			{
				nlohmann::json ids = nlohmann::json::array();
				for (const Pin& pin : pins)
				{
					ids.push_back(reinterpret_cast<wx::Size>(
						pin.id.AsPointer()));
				}
				return ids;
			};

			const wx::Size id = reinterpret_cast<wx::Size>(
				node->_id.AsPointer());
			document["nodes"][std::to_string(id)] = {
				{ "order", order++ },
				{ "type", node->_get_serialization_type() },
				{ "pins", {
					{ "inputs", serializePinIds(node->_input_pins) },
					{ "outputs", serializePinIds(node->_output_pins) }
				} },
				{ "data", node->_serialize() }
			};
		}

		for (const Link& link : _links)
		{
			const wx::Size id = reinterpret_cast<wx::Size>(
				link.id.AsPointer());
			document["links"][std::to_string(id)] = {
				{ "start_pin_id", reinterpret_cast<wx::Size>(
					link.start_pin_id.AsPointer()) },
				{ "end_pin_id", reinterpret_cast<wx::Size>(
					link.end_pin_id.AsPointer()) },
				{ "color", static_cast<ImU32>(link.color) }
			};
		}

		return document;
	}

	bool PopNodeEditor::Import(
		const nlohmann::json& document,
		PopNodeFactory& factory,
		std::string& error)
	{
		bool graphWasReplaced = false;
		auto fail = [this, &error, &graphWasReplaced](std::string message)
		{
			if (graphWasReplaced)
				_reset_editor({});
			error = std::move(message);
			return false;
		};

		if (!document.is_object())
			return fail(ML("project.invalid_structure"));

		const auto formatItr = document.find("format");
		if (formatItr == document.end() || !formatItr->is_string() ||
			formatItr->get_ref<const std::string&>() != "PopParticleEditor")
		{
			return fail(ML("project.unsupported_format"));
		}

		const auto versionItr = document.find("version");
		const bool versionMatches = versionItr != document.end() &&
			(versionItr->is_number_unsigned()
				? versionItr->get<wx::Uint64>() == 1
				: versionItr->is_number_integer() &&
					versionItr->get<wx::Int64>() == 1);
		if (!versionMatches)
		{
			return fail(ML("project.unsupported_version"));
		}

		const auto editorDataItr = document.find("node_editor");
		const auto nodeDataItr = document.find("nodes");
		const auto linkDataItr = document.find("links");
		if (editorDataItr == document.end() || !editorDataItr->is_object() ||
			nodeDataItr == document.end() || !nodeDataItr->is_object() ||
			linkDataItr == document.end() || !linkDataItr->is_object())
		{
			return fail(ML("project.invalid_structure"));
		}

		const auto& editorData = *editorDataItr;
		const auto& nodeData = *nodeDataItr;
		const auto& linkData = *linkDataItr;
		std::unordered_set<wx::SSize> usedIds;
		std::unordered_set<wx::SSize> pinIds;
		wx::SSize maxStoredId = 0;

		auto registerId = [&error, &usedIds, &maxStoredId](
			wx::Uint64 rawId,
			std::string_view description,
			wx::SSize& id)
		{
			if (rawId == 0 || rawId > static_cast<wx::Uint64>(
				std::numeric_limits<wx::SSize>::max()))
			{
				error = MLF("project.invalid_description", ML(description.data()));
				return false;
			}

			id = static_cast<wx::SSize>(rawId);
			if (!usedIds.emplace(id).second)
			{
				error = MLF(
					"project.duplicate_graph_id",
					static_cast<long long>(id));
				return false;
			}
			maxStoredId = std::max(maxStoredId, id);
			return true;
		};

		auto parseIdKey = [&error, &registerId](
			const std::string& key,
			std::string_view description,
			wx::SSize& id)
		{
			if (key.empty())
			{
				error = MLF("project.invalid_description", ML(description.data()));
				return false;
			}

			wx::Uint64 rawId = 0;
			for (const unsigned char character : key)
			{
				if (character < '0' || character > '9')
				{
					error = MLF("project.invalid_description", ML(description.data()));
					return false;
				}

				const wx::Uint64 digit = character - '0';
				if (rawId > (std::numeric_limits<wx::Uint64>::max() - digit) / 10)
				{
					error = MLF("project.invalid_description", ML(description.data()));
					return false;
				}
				rawId = rawId * 10 + digit;
			}
			return registerId(rawId, description, id);
		};

		auto parsePinIds = [&error, &registerId, &pinIds](
			const nlohmann::json& values,
			std::string_view description,
			std::vector<wx::SSize>& ids)
		{
			if (!values.is_array())
			{
				error = MLF("project.invalid_description_list", ML(description.data()));
				return false;
			}

			ids.clear();
			ids.reserve(values.size());
			for (const auto& value : values)
			{
				if (!value.is_number_unsigned())
				{
					error = MLF("project.invalid_description", ML(description.data()));
					return false;
				}

				wx::SSize id = 0;
				if (!registerId(
					value.get<wx::Uint64>(),
					description,
					id))
				{
					return false;
				}
				pinIds.emplace(id);
				ids.push_back(id);
			}
			return true;
		};

		std::vector<NodeImportData> pendingNodes;
		pendingNodes.reserve(nodeData.size());
		for (const auto& [key, value] : nodeData.items())
		{
			if (!value.is_object())
				return fail(ML("project.invalid_node_data"));

			wx::SSize id = 0;
			if (!parseIdKey(key, "node id", id))
				return fail(error);

			const auto typeItr = value.find("type");
			const auto dataItr = value.find("data");
			const auto pinsItr = value.find("pins");
			if (typeItr == value.end() || !typeItr->is_string() ||
				dataItr == value.end() || !dataItr->is_object() ||
				pinsItr == value.end() || !pinsItr->is_object())
			{
				return fail(ML("project.invalid_node_data"));
			}

			const auto inputsItr = pinsItr->find("inputs");
			const auto outputsItr = pinsItr->find("outputs");
			if (inputsItr == pinsItr->end() || outputsItr == pinsItr->end())
				return fail(ML("project.invalid_pin_data"));

			NodeImportData pending{};
			pending.id = id;
			pending.order = pendingNodes.size();
			const auto orderItr = value.find("order");
			if (orderItr != value.end())
			{
				if (!orderItr->is_number_unsigned() ||
					orderItr->get<wx::Uint64>() >
					static_cast<wx::Uint64>(
						std::numeric_limits<wx::Size>::max()))
				{
					return fail(ML("project.invalid_node_order"));
				}
				pending.order = static_cast<wx::Size>(
					orderItr->get<wx::Uint64>());
			}

			if (!parsePinIds(
				*inputsItr,
				"input pin id",
				pending.input_pin_ids) ||
				!parsePinIds(
					*outputsItr,
					"output pin id",
					pending.output_pin_ids))
			{
				return fail(error);
			}

			const std::string& type = typeItr->get_ref<const std::string&>();
			auto createdNode = factory.Instantiate(type);
			if (!createdNode.has_value())
				return fail(MLF("project.unknown_node_type", type.c_str()));

			pending.node = std::move(createdNode.value());
			try
			{
				pending.node->_deserialize(*dataItr);
			}
			catch (const nlohmann::json::exception&)
			{
				return fail(MLF("project.invalid_node_type_data", type.c_str()));
			}
			pendingNodes.emplace_back(std::move(pending));
		}

		std::sort(
			pendingNodes.begin(),
			pendingNodes.end(),
			[](const NodeImportData& lhs, const NodeImportData& rhs)
			{
				return lhs.order < rhs.order;
			});

		std::vector<LinkImportData> pendingLinks;
		pendingLinks.reserve(linkData.size());
		for (const auto& [key, value] : linkData.items())
		{
			if (!value.is_object())
				return fail(ML("project.invalid_link_data"));

			LinkImportData pending{};
			if (!parseIdKey(key, "link id", pending.id))
				return fail(error);

			const auto startItr = value.find("start_pin_id");
			const auto endItr = value.find("end_pin_id");
			if (startItr == value.end() || !startItr->is_number_unsigned() ||
				endItr == value.end() || !endItr->is_number_unsigned())
			{
				return fail(ML("project.invalid_link_pin"));
			}

			const wx::Uint64 rawStart = startItr->get<wx::Uint64>();
			const wx::Uint64 rawEnd = endItr->get<wx::Uint64>();
			if (rawStart > static_cast<wx::Uint64>(
					std::numeric_limits<wx::SSize>::max()) ||
				rawEnd > static_cast<wx::Uint64>(
					std::numeric_limits<wx::SSize>::max()))
			{
				return fail(ML("project.invalid_link_pin"));
			}
			pending.start_pin_id = static_cast<wx::SSize>(rawStart);
			pending.end_pin_id = static_cast<wx::SSize>(rawEnd);
			if (!pinIds.count(pending.start_pin_id) ||
				!pinIds.count(pending.end_pin_id))
			{
				return fail(ML("project.unknown_pin"));
			}

			pending.color = 0xFFFFFFFF;
			const auto colorItr = value.find("color");
			if (colorItr != value.end())
			{
				if (!colorItr->is_number_unsigned() ||
					colorItr->get<wx::Uint64>() >
					std::numeric_limits<wx::Uint32>::max())
				{
					return fail(ML("project.invalid_link_color"));
				}
				pending.color = static_cast<wx::Uint32>(
					colorItr->get<wx::Uint64>());
			}
			pendingLinks.emplace_back(pending);
		}

		const auto nextIdItr = editorData.find("next_id");
		if (nextIdItr != editorData.end())
		{
			if (!nextIdItr->is_number_unsigned() ||
				nextIdItr->get<wx::Uint64>() >
				static_cast<wx::Uint64>(
					std::numeric_limits<wx::SSize>::max()))
			{
				return fail(ML("project.invalid_next_id"));
			}
			maxStoredId = std::max(
				maxStoredId,
				static_cast<wx::SSize>(
					nextIdItr->get<wx::Uint64>()));
		}

		std::string settings;
		const auto settingsItr = editorData.find("settings_file");
		if (settingsItr != editorData.end())
		{
			if (settingsItr->is_string())
				settings = settingsItr->get_ref<const std::string&>();
			else if (settingsItr->is_object())
				settings = settingsItr->dump(
					-1,
					' ',
					false,
					nlohmann::json::error_handler_t::replace);
			else
				return fail(ML("project.invalid_editor_settings"));
		}

		_reset_editor(std::move(settings));
		graphWasReplaced = true;
		_next_id = static_cast<wx::Size>(maxStoredId);

		for (NodeImportData& pending : pendingNodes)
		{
			pending.node->_id = ed::NodeId(pending.id);
			Node* node = pending.node.get();
			_nodes_map[pending.id] = node;
			_nodes.emplace_back(std::move(pending.node));
			node->_on_initialize(*this);

			if (node->_input_pins.size() != pending.input_pin_ids.size() ||
				node->_output_pins.size() != pending.output_pin_ids.size())
			{
				return fail(MLF(
					"project.pin_layout_mismatch",
					node->_get_serialization_type().c_str()));
			}

			auto restorePinIds = [node](
				std::list<Pin>& pins,
				const std::vector<wx::SSize>& ids)
			{
				auto idItr = ids.begin();
				for (Pin& pin : pins)
				{
					pin.id = ed::PinId(*idItr++);
					pin.node_id = node->_id;
				}
			};
			restorePinIds(node->_input_pins, pending.input_pin_ids);
			restorePinIds(node->_output_pins, pending.output_pin_ids);
		}

		_pins_map.clear();
		for (const auto& node : _nodes)
		{
			for (Pin& pin : node->_input_pins)
			{
				_pins_map[reinterpret_cast<wx::Size>(
					pin.id.AsPointer())] = &pin;
			}
			for (Pin& pin : node->_output_pins)
			{
				_pins_map[reinterpret_cast<wx::Size>(
					pin.id.AsPointer())] = &pin;
			}
		}

		for (const LinkImportData& pending : pendingLinks)
		{
			const auto startItr = _pins_map.find(pending.start_pin_id);
			const auto endItr = _pins_map.find(pending.end_pin_id);
			if (startItr == _pins_map.end() || endItr == _pins_map.end())
				return fail(ML("project.unknown_pin"));
			if (!Pin::_can_create_link(*startItr->second, *endItr->second))
				return fail(ML("project.invalid_link"));

			_links.push_back({
				ed::LinkId(pending.id),
				ed::PinId(pending.start_pin_id),
				ed::PinId(pending.end_pin_id),
				ImColor(pending.color)
			});
			startItr->second->connected_links.push_back(&_links.back());
			endItr->second->connected_links.push_back(&_links.back());
		}

		_next_id = static_cast<wx::Size>(maxStoredId);
		error.clear();
		return true;
	}

	PopNodeEditor::Node::Node()
		: _id(0),
		_type(NodeType::Normal)
	{

	}

	void PopNodeEditor::Node::_on_render()
	{
		_tooltip.clear();

		ed::BeginNode(_id);
		ImGui::PushID(_id.AsPointer());

		ImGui::BeginGroup();

		ImGui::TextUnformatted(ML(_name.c_str()));

		wx::Float32 bottomHeader = ImGui::GetItemRectMax().y + 5.0f;

		ImGui::EndGroup();

		ImGui::Dummy({ 0.0f, 5.0f });

		// input pins
		ImGui::BeginGroup();
		for (auto& pin : _input_pins)
		{
			_render_pin(pin);
		}
		ImGui::EndGroup();

		ImGui::SameLine();

		wx::Float32 tempY = ImGui::GetCursorScreenPos().y;

		ImGui::BeginGroup();
		ImGui::Dummy({ 50,0 });
		_on_render_workspace();
		ImGui::EndGroup();

		ImGui::SameLine();

		wx::Int32 widthMax = -1;

		for (auto& pin : _output_pins)
		{
			widthMax = std::max(_calc_pin_width(pin), widthMax);
		}

		ImGui::BeginGroup();

		for (auto& pin : _output_pins)
		{
			ImGui::Dummy({ static_cast<wx::Float32>(widthMax) - _calc_pin_width(pin),0.0f});
			ImGui::SameLine();
			_render_pin(pin);
		}

		ImGui::EndGroup();

		ImGui::Dummy({ 0,30.0f });
		ImGui::PopID();
		ed::EndNode();

		ImDrawList* drawList = ed::GetNodeBackgroundDrawList(_id);
		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();

		drawList->AddRectFilled(nodeMin,
			{ nodeMax.x, bottomHeader },
			_header_color,
			ed::GetStyle().NodeRounding,
			ImDrawFlags_RoundCornersTop);
	}

	void PopNodeEditor::Node::_on_render_workspace()
	{
	}

	void PopNodeEditor::Node::_on_render_overlay()
	{
		if (_tooltip.empty())
			return;

		ImGui::BeginTooltip();
		ImGui::TextUnformatted(_tooltip.c_str());
		ImGui::EndTooltip();
	}

	PopNodeEditor::Pin& PopNodeEditor::Node::_add_pin(
		PopNodeEditor& editor,
		const Pin& pin)
	{
		return editor._add_node_pin(*this, pin);
	}

	PopNodeEditor::Node* PopNodeEditor::Node::_find_node(
		PopNodeEditor& editor,
		std::string_view name)
	{
		return editor._find_node(name, this);
	}

	void PopNodeEditor::Node::_set_item_tooltip(std::string_view text)
	{
		if (ImGui::IsItemHovered(
			ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
		{
			_tooltip.assign(text.data(), text.size());
		}
	}

	PopNodeEditor::Pin& PopNodeEditor::Node::_store_pin(const Pin& pin, ax::NodeEditor::PinId id)
	{
		if (pin.kind == ed::PinKind::Input)
		{
			_input_pins.push_back(pin);
			_input_pins.back().id = id;
			_input_pins.back().node_id = _id;
			return _input_pins.back();
		}
		else
		{
			_output_pins.push_back(pin);
			_output_pins.back().id = id;
			_output_pins.back().node_id = _id;
			return _output_pins.back();
		}
	}

	const ax::NodeEditor::NodeId& PopNodeEditor::Node::_get_id() const
	{
		return _id;
	}

	const std::string& PopNodeEditor::Node::_get_serialization_type() const noexcept
	{
		return _serialization_type;
	}

	void PopNodeEditor::Node::_render_pin(Pin& pin)
	{
		ed::BeginPin(pin.id, pin.kind);

		ImGui::Dummy({ 0,ImGui::GetFrameHeight() });
		ImGui::SameLine();

		ImVec2 pinCenter{};

		ImGui::AlignTextToFramePadding();

		if (pin.kind == ed::PinKind::Input)
		{
			const ImVec2 pinPosition = ImGui::GetCursorScreenPos();
			pinCenter = { pinPosition.x + 9.0f, pinPosition.y + 14.0f };
			PopNodeEditor::_draw_pin_shape(pin, pin.connected_links.size());
			ImGui::SameLine();
			ImGui::TextUnformatted(ML(pin.name.c_str()));
		}
		else
		{
			ImGui::TextUnformatted(ML(pin.name.c_str()));
			ImGui::SameLine();
			const ImVec2 pinPosition = ImGui::GetCursorScreenPos();
			pinCenter = { pinPosition.x + 9.0f, pinPosition.y + 14.0f };
			PopNodeEditor::_draw_pin_shape(pin, pin.connected_links.size());
		}

		pin.layout_y = ed::ScreenToCanvas(pinCenter).y -
			ed::GetNodePosition(_id).y;
		
		ed::EndPin();
	}

	wx::Int32 PopNodeEditor::Node::_calc_pin_width(const Pin& pin)
	{
		return static_cast<wx::Int32>(ImGui::CalcTextSize(ML(pin.name.c_str())).x + 18.0f/* shape size */);
	}

	void PopNodeFactory::Register(std::string_view type, const NodeCreator& creator)
	{
		_creators[type.data()] = creator;
	}

	std::optional<std::unique_ptr<PopNodeEditor::Node>> PopNodeFactory::Instantiate(std::string_view type)
	{
		if (!_creators.count(type.data()))
			return std::nullopt;
		return _creators[type.data()]();
	}
}
