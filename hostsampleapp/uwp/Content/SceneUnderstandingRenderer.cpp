//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"

#include "../Common/DbgLog.h"
#include "../Common/DirectXHelper.h"

#include "SceneUnderstandingRenderer.h"

#include <winrt/Windows.Perception.Spatial.Preview.h>

using namespace RemotingHostSample;

namespace
{
    using namespace DirectX;

    void AppendColoredTriangle(
        winrt::Windows::Foundation::Numerics::float3 p0,
        winrt::Windows::Foundation::Numerics::float3 p1,
        winrt::Windows::Foundation::Numerics::float3 p2,
        winrt::Windows::Foundation::Numerics::float3 color,
        std::vector<VertexPositionNormalColor>& vertices)
    {
        VertexPositionNormalColor vertex;
        vertex.color = XMFLOAT3(&color.x);
        vertex.normal = XMFLOAT3(0.0f, 0.0f, 0.0f);

        vertex.pos = XMFLOAT3(&p0.x);
        vertices.push_back(vertex);
        vertex.pos = XMFLOAT3(&p1.x);
        vertices.push_back(vertex);
        vertex.pos = XMFLOAT3(&p2.x);
        vertices.push_back(vertex);
    }
} // namespace


// Struct to hold one entity label type entry
struct Label
{
    const wchar_t* const Name;
    uint32_t Index;
    uint8_t RGB[3];
};

// Entity label types
static const Label LabelStorage[] = {
    {L"Background", 0, {243, 121, 223}},
    {L"Ignore", 255, {255, 255, 255}},
    {L"Wall", 1, {243, 126, 121}},
    {L"Floor", 2, {187, 243, 121}},
    {L"Ceiling", 3, {121, 152, 243}},
    {L"Table", 4, {121, 243, 227}},
    {L"Chair", 5, {243, 191, 121}},
    {L"Window", 6, {121, 243, 146}},
    {L"Door", 7, {156, 121, 243}},
    {L"Monitor", 8, {2, 159, 253}},
    {L"Pillar", 10, {253, 106, 2}},
    {L"Couch", 11, {72, 197, 126}},
    {L"Whiteboard", 12, {137, 159, 2}},
    {L"Beanbag", 13, {206, 112, 74}},
    {L"Cabinet", 14, {36, 43, 138}},
    {L"Nightstands", 15, {78, 231, 210}},
    {L"TVStands", 16, {26, 71, 66}},
    {L"Countertops", 17, {13, 60, 55}},
    {L"Dressers", 18, {29, 58, 55}},
    {L"Bench", 19, {105, 54, 136}},
    {L"Ottoman", 20, {99, 9, 44}},
    {L"Stool", 21, {255, 204, 153}},
    {L"GTEquipment", 22, {206, 199, 74}},
    {L"Telephone", 23, {243, 217, 121}},
    {L"Bookshelf", 24, {37, 117, 164}},
    {L"Laptop", 25, {96, 147, 234}},
    {L"Stanchion", 26, {29, 117, 40}},
    {L"Markers", 27, {111, 93, 167}},
    {L"Controller", 28, {230, 254, 251}},
    {L"Stairs", 9, {43, 174, 100}},
    {L"Empty", 254, {0, 0, 0}},
    {L"Appliances-CeilingLight", 30, {250, 24, 180}},
    {L"Appliances-DishWasher", 32, {38, 204, 168}},
    {L"Appliances-FloorLamp", 34, {106, 134, 187}},
    {L"Appliances-Lighting", 36, {156, 162, 56}},
    {L"Appliances-Microwave", 37, {6, 44, 91}},
    {L"Appliances-NotSpecified", 38, {35, 188, 199}},
    {L"Appliances-Oven", 39, {153, 60, 52}},
    {L"Appliances-SmallAppliances", 40, {255, 83, 112}},
    {L"Appliances-Stove", 41, {76, 175, 147}},
    {L"Appliances-Toaster", 42, {145, 58, 23}},
    {L"Appliances-WashingMachine", 44, {46, 66, 12}},
    {L"Appliances-DeskLamp", 45, {128, 86, 177}},
    {L"Appliances-Dryer", 46, {239, 162, 164}},
    {L"Appliances-Fridge", 47, {87, 243, 139}},
    {L"Appliances-WallLight", 50, {222, 49, 1}},
    {L"Bed-BunkBed", 51, {97, 174, 71}},
    {L"Bed-DoubleBed", 52, {85, 195, 111}},
    {L"Bed-NotSpecified", 53, {212, 26, 75}},
    {L"Bed-SingleBed", 54, {200, 219, 241}},
    {L"Ceiling-Unassigned", 55, {48, 120, 115}},
    {L"Ceiling-NotSpecified", 56, {205, 144, 139}},
    {L"Chair-Beanbag", 57, {136, 175, 192}},
    {L"Chair-Bench", 58, {89, 41, 203}},
    {L"Chair-ArmChair", 59, {192, 1, 27}},
    {L"Chair-ArmOfAChair", 60, {194, 241, 101}},
    {L"Chair-BarStool", 61, {146, 21, 8}},
    {L"Chair-ChaiseLounge", 62, {178, 31, 121}},
    {L"Chair-DiningChair", 63, {76, 10, 219}},
    {L"Chair-LoungeChair", 64, {174, 165, 77}},
    {L"Chair-NotSpecified", 65, {186, 217, 58}},
    {L"Chair-OfficeChair", 66, {177, 29, 181}},
    {L"Chair-Unknown", 67, {155, 128, 196}},
    {L"Chair-Ottoman", 68, {28, 75, 247}},
    {L"Chair-Stool", 69, {60, 243, 241}},
    {L"Door-DoubleDoors", 70, {220, 101, 83}},
    {L"Door-NotSpecified", 71, {219, 20, 187}},
    {L"Door-Revolving", 72, {211, 229, 158}},
    {L"Door-SingleDoor", 73, {10, 100, 12}},
    {L"Door-Sliding", 74, {73, 197, 108}},
    {L"Electronics-Desktop", 75, {181, 22, 191}},
    {L"Electronics-DVDPlayer", 76, {5, 131, 13}},
    {L"Electronics-Headphones", 77, {169, 60, 180}},
    {L"Electronics-Keyboard", 78, {6, 92, 79}},
    {L"Electronics-Laptop", 79, {252, 108, 50}},
    {L"Electronics-Mobile", 80, {35, 73, 64}},
    {L"Electronics-Mouse", 81, {3, 112, 214}},
    {L"Electronics-Mousepad", 82, {106, 70, 62}},
    {L"Electronics-NotSpecified", 83, {63, 100, 209}},
    {L"Electronics-Phone", 84, {64, 32, 142}},
    {L"Electronics-Printer", 85, {70, 188, 0}},
    {L"Electronics-Projector", 86, {72, 100, 38}},
    {L"Electronics-Speakers", 87, {202, 60, 135}},
    {L"Electronics-Tablet", 88, {126, 2, 49}},
    {L"Electronics-TVMonitor", 89, {188, 184, 46}},
    {L"Electronics-Xbox", 90, {6, 218, 26}},
    {L"Electronics-Monitor", 91, {179, 160, 177}},
    {L"Floor-Unassigned", 92, {9, 42, 145}},
    {L"Human-Female", 93, {52, 156, 230}},
    {L"Human-Male", 94, {231, 88, 138}},
    {L"Human-Other", 95, {0, 0, 255}},
    {L"NotSpecified-Ax", 96, {230, 228, 24}},
    {L"NotSpecified-Backpack", 97, {228, 104, 245}},
    {L"NotSpecified-Bag", 98, {215, 41, 202}},
    {L"NotSpecified-Barbell", 99, {100, 125, 112}},
    {L"NotSpecified-BlackBoard", 100, {65, 166, 116}},
    {L"NotSpecified-Bottle", 101, {140, 68, 191}},
    {L"NotSpecified-box", 102, {145, 146, 89}},
    {L"NotSpecified-Cable", 103, {170, 1, 118}},
    {L"NotSpecified-Can", 104, {205, 195, 201}},
    {L"NotSpecified-Cart", 105, {156, 159, 0}},
    {L"NotSpecified-case", 106, {208, 70, 137}},
    {L"NotSpecified-CeilingFan", 107, {9, 227, 245}},
    {L"NotSpecified-Clothes", 108, {181, 123, 192}},
    {L"NotSpecified-Coat", 109, {189, 249, 62}},
    {L"NotSpecified-Coatrack", 110, {136, 15, 19}},
    {L"NotSpecified-CorkBoard", 111, {167, 98, 139}},
    {L"NotSpecified-CounterTop", 112, {6, 14, 93}},
    {L"NotSpecified-Drawers", 113, {216, 156, 242}},
    {L"NotSpecified-Drinkcontainer", 114, {238, 153, 75}},
    {L"NotSpecified-Dumbbell", 115, {183, 111, 41}},
    {L"NotSpecified-ElectricalOutlet", 116, {191, 199, 36}},
    {L"NotSpecified-ElectricalSwitch", 117, {31, 81, 127}},
    {L"NotSpecified-Elliptical", 118, {244, 92, 59}},
    {L"NotSpecified-Food", 119, {221, 210, 211}},
    {L"NotSpecified-Footwear", 120, {163, 245, 159}},
    {L"NotSpecified-Hammer", 121, {118, 176, 85}},
    {L"NotSpecified-LaptopBag", 122, {225, 32, 60}},
    {L"NotSpecified-LIDAR", 123, {26, 105, 172}},
    {L"NotSpecified-Mannequin", 124, {131, 135, 194}},
    {L"NotSpecified-Markers", 125, {124, 23, 155}},
    {L"NotSpecified-Microscope", 126, {128, 143, 248}},
    {L"NotSpecified-NDI", 127, {220, 39, 237}},
    {L"NotSpecified-Pinwheel", 128, {155, 24, 46}},
    {L"NotSpecified-PunchingBag", 129, {152, 215, 122}},
    {L"NotSpecified-Shower", 130, {78, 243, 86}},
    {L"NotSpecified-Sign", 131, {29, 159, 136}},
    {L"NotSpecified-Sink", 132, {209, 19, 236}},
    {L"NotSpecified-Sissors", 133, {31, 229, 162}},
    {L"NotSpecified-Sphere", 134, {151, 86, 155}},
    {L"NotSpecified-StairClimber", 135, {52, 236, 130}},
    {L"NotSpecified-stanchion", 136, {6, 76, 221}},
    {L"NotSpecified-Stand", 137, {2, 12, 172}},
    {L"NotSpecified-StationaryBike", 138, {69, 190, 196}},
    {L"NotSpecified-Tape", 139, {176, 3, 131}},
    {L"NotSpecified-Thermostat", 140, {33, 22, 47}},
    {L"NotSpecified-Toilet", 141, {107, 45, 152}},
    {L"NotSpecified-TrashCan", 142, {128, 72, 143}},
    {L"NotSpecified-Tripod", 143, {225, 31, 162}},
    {L"NotSpecified-Tub", 144, {110, 147, 77}},
    {L"NotSpecified-Vent", 145, {137, 170, 110}},
    {L"NotSpecified-WeightBench", 146, {183, 79, 90}},
    {L"NotSpecified-Wire", 147, {0, 255, 38}},
    {L"NotSpecified-Wrench", 148, {116, 3, 22}},
    {L"NotSpecified-Pillar", 149, {128, 184, 144}},
    {L"NotSpecified-Whiteboard", 150, {94, 240, 206}},
    {L"Plant-Fake", 151, {216, 230, 169}},
    {L"Plant-NotSpecified", 152, {182, 43, 63}},
    {L"Plant-Organic", 153, {197, 86, 148}},
    {L"Props-Book", 154, {247, 3, 157}},
    {L"Props-Cushion", 155, {13, 94, 49}},
    {L"Props-FloorVase", 156, {55, 213, 231}},
    {L"Props-FlowerPot", 157, {239, 172, 43}},
    {L"Props-Magazine", 158, {138, 164, 178}},
    {L"Props-Mirror", 159, {116, 236, 157}},
    {L"Props-NewsPaper", 160, {62, 80, 43}},
    {L"Props-NotSpecified", 161, {9, 106, 45}},
    {L"Props-Paintings", 162, {164, 117, 118}},
    {L"Props-PaperSheet", 163, {85, 190, 229}},
    {L"Props-PhotoFrame", 164, {18, 95, 80}},
    {L"Props-Rug", 165, {192, 82, 167}},
    {L"Props-Sculpture", 166, {130, 15, 64}},
    {L"Props-Toys", 167, {136, 130, 225}},
    {L"Sofa-ChaiseLounge", 168, {241, 154, 12}},
    {L"Sofa-NotSpecified", 169, {113, 197, 139}},
    {L"Sofa-Sectional", 170, {24, 132, 64}},
    {L"Sofa-Straight", 171, {248, 137, 194}},
    {L"Storage-Bookshelf", 172, {4, 69, 174}},
    {L"Storage-ChinaCabinet", 173, {216, 165, 83}},
    {L"Storage-Dresser", 174, {156, 24, 110}},
    {L"Storage-FileCabinet", 175, {78, 78, 12}},
    {L"Storage-MediaCabinet", 176, {168, 234, 45}},
    {L"Storage-NotSpecified", 177, {29, 232, 238}},
    {L"Storage-Rack", 178, {161, 36, 92}},
    {L"Storage-Shelf", 179, {57, 187, 87}},
    {L"Storage-Cabinet", 180, {164, 23, 45}},
    {L"Storage-Stairs", 181, {10, 13, 61}},
    {L"Table-CoffeeTable", 182, {178, 214, 30}},
    {L"Table-ConferenceTable", 183, {25, 153, 182}},
    {L"Table-Desk", 184, {171, 128, 231}},
    {L"Table-DiningTable", 185, {12, 169, 156}},
    {L"Table-Nightstand", 186, {247, 131, 122}},
    {L"Table-NotSpecified", 187, {227, 214, 90}},
    {L"Table-OfficeDesk", 188, {122, 253, 7}},
    {L"Table-OfficeTable", 189, {6, 20, 5}},
    {L"Table-SideTable", 190, {230, 211, 253}},
    {L"Unassigned-Unassigned", 191, {141, 204, 180}},
    {L"Utensils-Bowl", 192, {108, 89, 46}},
    {L"Utensils-Cups", 193, {90, 250, 131}},
    {L"Utensils-Knife", 194, {28, 67, 176}},
    {L"Utensils-Mug", 195, {152, 218, 150}},
    {L"Utensils-NotSpecified", 196, {211, 96, 157}},
    {L"Utensils-Pans", 197, {73, 159, 109}},
    {L"Utensils-Pots", 198, {7, 193, 112}},
    {L"Utensils-Tray", 199, {60, 152, 1}},
    {L"Vehicle-Car", 200, {189, 149, 61}},
    {L"Vehicle-MotorCycle", 201, {2, 164, 102}},
    {L"Vehicle-Segway", 202, {198, 165, 85}},
    {L"Vehicle-Truck", 203, {134, 46, 106}},
    {L"Wall-Blinds", 204, {9, 13, 13}},
    {L"Wall-Curtain", 205, {52, 74, 241}},
    {L"Wall-Unassigned", 206, {83, 158, 59}},
    {L"Wall-Window", 207, {117, 162, 84}},
    {L"Storage-BathroomVanity", 208, {127, 151, 35}},
    {L"NotSpecified-Unassigned", 209, {143, 133, 123}},
    {L"Storage-Nightstand", 210, {181, 112, 177}},
    {L"Storage-Unassigned", 211, {73, 125, 140}},
    {L"Props-Unassigned", 212, {156, 127, 134}},
    {L"Storage-ArmChair", 213, {102, 111, 19}},
    {L"NotSpecified-LaundryBasket", 214, {106, 168, 192}},
    {L"Props-Decorations", 215, {49, 242, 177}},
    {L"NotSpecified-Fireplace", 216, {96, 128, 236}},
    {L"NotSpecified-Drinkware", 217, {6, 247, 22}},
    {L"Sofa-LoungeChair", 218, {167, 92, 66}},
    {L"NotSpecified-NotSpecified", 219, {174, 127, 40}},
    {L"Mouse", 220, {65, 33, 210}},
    {L"Bag", 221, {168, 71, 185}},
    {L"Fridge", 222, {255, 127, 94}},
    {L"Stand", 223, {246, 160, 193}},
    {L"Sign", 224, {143, 221, 54}},
    {L"Sphere", 225, {255, 207, 172}},
    {L"Tripod", 227, {255, 235, 46}},
    {L"PinWheel", 228, {13, 92, 139}},
    {L"Kart", 229, {49, 3, 27}},
    {L"Box", 230, {134, 215, 144}},
    {L"Light", 231, {140, 3, 56}},
    {L"Keyboard ", 232, {7, 66, 58}},
    {L"Scupture", 233, {240, 191, 82}},
    {L"Lamp", 234, {189, 8, 78}},
    {L"Microscope ", 235, {255, 211, 112}},
    {L"Case ", 236, {59, 155, 70}},
    {L"Ax", 237, {157, 117, 29}},
    {L"Manikin_Parts ", 238, {67, 141, 186}},
    {L"Clothing ", 239, {4, 122, 55}},
    {L"CoatRack", 240, {211, 52, 114}},
    {L"DrinkContainer ", 241, {35, 23, 0}},
    {L"MousePad", 242, {68, 28, 0}},
    {L"Tape", 243, {107, 173, 211}},
    {L"Sissors ", 245, {53, 24, 143}},
    {L"Headphones ", 246, {45, 212, 189}},
};

constexpr auto NumLabels = sizeof(LabelStorage) / sizeof(Label);

// Dictionary to quickly access labels by numeric label ID
using LabelDictionary = std::map<uint32_t, const Label*>;
static LabelDictionary Labels;

SceneUnderstandingRenderer::SceneUnderstandingRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : RenderableObject(deviceResources)
{
    // If the label dictionary is still empty, build it from static data
    if (Labels.empty())
    {
        for (size_t i = 0; i < NumLabels; ++i)
        {
            Labels[LabelStorage[i].Index] = &LabelStorage[i];
        }
    }
}

void SceneUnderstandingRenderer::Update(
    winrt::SceneUnderstanding::SceneProcessor& sceneProcessor,
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
    winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation)
{
    m_vertices.clear();

    // Calculate the head position at the time of the last SU update in render space. This information can be
    // used for debug rendering.
    winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float3> lastUpdatePosInRenderSpace;
    if (lastUpdateLocation)
    {
        auto lastUpdateCS = lastUpdateLocation.CoordinateSystem();
        auto lastUpdateLocationToRender = lastUpdateCS.TryGetTransformTo(renderingCoordinateSystem);
        if (lastUpdateLocationToRender)
        {
            lastUpdatePosInRenderSpace = winrt::Windows::Foundation::Numerics::transform({0, 0, 0}, lastUpdateLocationToRender.Value());
        }
    }

    // Lambda to execute for each quad returned by SU. Adds the quad to the vertex buffer for rendering, using
    // the color indicated by the label dictionary for the quad's owner entity's type. Optionally draws debug
    // rays from the last update position to each quad.
    auto processQuadForRendering = [this, &renderingCoordinateSystem, &lastUpdatePosInRenderSpace](
                                       const winrt::SceneUnderstanding::Entity& entity,
                                       const winrt::SceneUnderstanding::Quad& quad,
                                       const winrt::Windows::Foundation::Numerics::float4x4& entityToAnchorTransform,
                                       const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& entityAnchorCS) {
        // Determine the transform to go from entity space to rendering space
        winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float4x4> anchorToRenderingRef =
            entityAnchorCS.TryGetTransformTo(renderingCoordinateSystem);
        if (!anchorToRenderingRef)
        {
            return;
        }
        winrt::Windows::Foundation::Numerics::float4x4 anchorToRenderingTransform = anchorToRenderingRef.Value();
        winrt::Windows::Foundation::Numerics::float4x4 entityToRenderingTransform = entityToAnchorTransform * anchorToRenderingTransform;

        // Create the quad's corner points in entity space and transform them to rendering space
        const float width = quad.WidthInMeters();
        const float height = quad.HeightInMeters();
        winrt::Windows::Foundation::Numerics::float3 positions[4] = {
            {-width / 2, -height / 2, 0.0f}, {width / 2, -height / 2, 0.0f}, {-width / 2, height / 2, 0.0f}, {width / 2, height / 2, 0.0f}};
        for (int i = 0; i < 4; ++i)
        {
            positions[i] = winrt::Windows::Foundation::Numerics::transform(positions[i], entityToRenderingTransform);
        }

        // Determine the color with which to draw the quad
        winrt::Windows::Foundation::Numerics::float3 color{1.0f, 1.0f, 0.0f};
        auto labelPos = Labels.find(static_cast<uint32_t>(entity.Label()));
        if (labelPos != Labels.end())
        {
            const Label& label = *labelPos->second;
            color = {label.RGB[0] / 255.0f, label.RGB[1] / 255.0f, label.RGB[2] / 255.0f};
        }

        // Add triangles to render the quad (both winding orders to guarantee double-sided rendering)
        AppendColoredTriangle(positions[0], positions[3], positions[1], color, m_vertices);
        AppendColoredTriangle(positions[0], positions[2], positions[3], color, m_vertices);
        AppendColoredTriangle(positions[1], positions[3], positions[0], color, m_vertices);
        AppendColoredTriangle(positions[3], positions[2], positions[0], color, m_vertices);

        /** /
        // Debug code: draw a ray from the last update position to the center of each visible quad
        if (lastUpdatePosInRenderSpace)
        {
            float rayEndRadius = 0.05f;
            winrt::Windows::Foundation::Numerics::float3 rayEndPositions[4] = {
                {-rayEndRadius, 0.0f, 0.0f},
                {rayEndRadius, 0.0f, 0.0f},
                {0.0f, -rayEndRadius, 0.0f},
                {0.0f, rayEndRadius, 0.0f},
            };
            for (int i = 0; i < 4; ++i)
            {
                rayEndPositions[i] = winrt::Windows::Foundation::Numerics::transform(rayEndPositions[i], entityToRenderingTransform);
            }
            AppendColoredTriangle(lastUpdatePosInRenderSpace.Value(), rayEndPositions[0], rayEndPositions[1], color, m_vertices);
            AppendColoredTriangle(lastUpdatePosInRenderSpace.Value(), rayEndPositions[1], rayEndPositions[0], color, m_vertices);
            AppendColoredTriangle(lastUpdatePosInRenderSpace.Value(), rayEndPositions[2], rayEndPositions[3], color, m_vertices);
            AppendColoredTriangle(lastUpdatePosInRenderSpace.Value(), rayEndPositions[3], rayEndPositions[2], color, m_vertices);
        }
        /**/
    };

    // Execute the above lambda for each quad known by the SceneProcessor
    ForEachQuad(sceneProcessor, processQuadForRendering);

    // The geometry we added is already in rendering space, so the model transform must be identity.
    auto modelTransform = winrt::Windows::Foundation::Numerics::float4x4::identity();
    UpdateModelConstantBuffer(modelTransform);
}

void SceneUnderstandingRenderer::DebugLogState(
    winrt::SceneUnderstanding::SceneProcessor& sceneProcessor,
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
    winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation)
{
    // Calculate the head position at the time of the last SU update in render space. This information can be
    // used for debug rendering.
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem lastUpdateCS = lastUpdateLocation.CoordinateSystem();
    winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float3> lastUpdatePosInRenderSpaceRef;
    if (lastUpdateLocation)
    {
        auto lastUpdateLocationToRender = lastUpdateCS.TryGetTransformTo(renderingCoordinateSystem);
        if (lastUpdateLocationToRender)
        {
            lastUpdatePosInRenderSpaceRef = winrt::Windows::Foundation::Numerics::transform({0, 0, 0}, lastUpdateLocationToRender.Value());
        }
    }
    if (!lastUpdatePosInRenderSpaceRef)
    {
        return;
    }
    winrt::Windows::Foundation::Numerics::float3 lastUpdatePosInRenderSpace = lastUpdatePosInRenderSpaceRef.Value();

    auto logQuad = [this, &lastUpdateCS](
                       const winrt::SceneUnderstanding::Entity& entity,
                       const winrt::SceneUnderstanding::Quad& quad,
                       const winrt::Windows::Foundation::Numerics::float4x4& entityToAnchorTransform,
                       const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& entityAnchorCS) {
        // Determine transform from entity space to last update pose space
        winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float4x4> anchorToLastUpdateRef =
            entityAnchorCS.TryGetTransformTo(lastUpdateCS);
        if (!anchorToLastUpdateRef)
        {
            return;
        }
        winrt::Windows::Foundation::Numerics::float4x4 anchorToLastUpdateTransform = anchorToLastUpdateRef.Value();
        winrt::Windows::Foundation::Numerics::float4x4 entityToLastUpdateTransform = entityToAnchorTransform * anchorToLastUpdateTransform;

        // Determine various sizes, position, and distance from head
        const float width = quad.WidthInMeters();
        const float height = quad.HeightInMeters();
        const float radius = sqrtf(width * width + height * height) / 2;

        const winrt::Windows::Foundation::Numerics::float3 position = winrt::Windows::Foundation::Numerics::transform(
            winrt::Windows::Foundation::Numerics::float3::zero(), entityToLastUpdateTransform);
        const float distance = winrt::Windows::Foundation::Numerics::length(position);

        const wchar_t* labelName = L"<unknown>";
        auto labelPos = Labels.find(static_cast<uint32_t>(entity.Label()));
        if (labelPos != Labels.end())
        {
            const Label& label = *labelPos->second;
            labelName = label.Name;
        }

        DebugLog(
            L"    %s (%.2f x %.2f m, radius: %.2f m) at %.2f;%.2f;%.2f (distance: %.2f m)",
            labelName,
            width,
            height,
            radius,
            position.x,
            position.y,
            position.z,
            distance);
    };

    DebugLog(L"--- SU Update ---");
    DebugLog(
        L"  Update position (in root space): (%.2f; %.2f; %.2f)",
        lastUpdatePosInRenderSpace.x,
        lastUpdatePosInRenderSpace.y,
        lastUpdatePosInRenderSpace.z);
    DebugLog(L"  Quads (in head pose space):");
    ForEachQuad(sceneProcessor, logQuad);
}

void SceneUnderstandingRenderer::Draw(unsigned int numInstances)
{
    if (m_vertices.empty())
    {
        return;
    }

    const UINT stride = sizeof(m_vertices[0]);
    const UINT offset = 0;
    D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
    vertexBufferData.pSysMem = m_vertices.data();
    const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(m_vertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
    winrt::com_ptr<ID3D11Buffer> vertexBuffer;
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer.put()));

    auto context = m_deviceResources->GetD3DDeviceContext();
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11Buffer* pBuffer = vertexBuffer.get();
    context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);
    context->DrawInstanced(static_cast<UINT>(m_vertices.size()), numInstances, offset, 0);
}

template <typename Func>
void SceneUnderstandingRenderer::ForEachQuad(winrt::SceneUnderstanding::SceneProcessor& sceneProcessor, Func f)
{
    // Collect all components, then iterate to find quad entities
    winrt::com_array<winrt::SceneUnderstanding::Component> components;
    sceneProcessor.GetAllComponents(components);
    for (auto& component : components)
    {
        winrt::SceneUnderstanding::Entity entity = component.try_as<winrt::SceneUnderstanding::Entity>();
        if (!entity)
        {
            continue;
        }

        winrt::SceneUnderstanding::Quad quad{nullptr};
        winrt::SceneUnderstanding::Transform transform{nullptr};
        winrt::SceneUnderstanding::SpatialCoordinateSystem spatialCS{nullptr};

        winrt::com_array<winrt::SceneUnderstanding::Id> associatedComponentIds;
        entity.GetAllAssociatedComponentIds(associatedComponentIds);

        for (auto& id : associatedComponentIds)
        {
            winrt::SceneUnderstanding::Component ac = sceneProcessor.GetComponent(id);
            if (auto q = ac.try_as<winrt::SceneUnderstanding::Quad>())
            {
                quad = q;
                continue;
            }
            if (auto t = ac.try_as<winrt::SceneUnderstanding::Transform>())
            {
                transform = t;
                continue;
            }
            if (auto s = ac.try_as<winrt::SceneUnderstanding::SpatialCoordinateSystem>())
            {
                spatialCS = s;
                continue;
            }
        }

        // Don't proceed if any essential bit of data is missing
        if (!quad || !transform || !spatialCS)
        {
            continue;
        }

        // Determine the transform from the entity to its anchor
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem entityAnchorCS{nullptr};
        try
        {
            entityAnchorCS = winrt::Windows::Perception::Spatial::Preview::SpatialGraphInteropPreview::CreateCoordinateSystemForNode(
                spatialCS.SpatialCoordinateGuid());
        }
        catch (const winrt::hresult_error&)
        {
            continue;
        }

        winrt::Windows::Foundation::Numerics::float4x4 entityToAnchorTransform = transform.TransformationMatrix();

        f(entity, quad, entityToAnchorTransform, entityAnchorCS);
    }
}
