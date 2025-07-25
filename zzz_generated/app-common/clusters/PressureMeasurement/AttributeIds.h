// DO NOT EDIT MANUALLY - Generated file
//
// Identifier constant values for cluster PressureMeasurement (cluster code: 1027/0x403)
// based on src/controller/data_model/controller-clusters.matter
#pragma once

#include <clusters/shared/GlobalIds.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {
namespace Clusters {
namespace PressureMeasurement {
namespace Attributes {

// Total number of attributes supported by the cluster, including global attributes
inline constexpr uint32_t kAttributesCount = 14;

namespace MeasuredValue {
inline constexpr AttributeId Id = 0x00000000;
} // namespace MeasuredValue

namespace MinMeasuredValue {
inline constexpr AttributeId Id = 0x00000001;
} // namespace MinMeasuredValue

namespace MaxMeasuredValue {
inline constexpr AttributeId Id = 0x00000002;
} // namespace MaxMeasuredValue

namespace Tolerance {
inline constexpr AttributeId Id = 0x00000003;
} // namespace Tolerance

namespace ScaledValue {
inline constexpr AttributeId Id = 0x00000010;
} // namespace ScaledValue

namespace MinScaledValue {
inline constexpr AttributeId Id = 0x00000011;
} // namespace MinScaledValue

namespace MaxScaledValue {
inline constexpr AttributeId Id = 0x00000012;
} // namespace MaxScaledValue

namespace ScaledTolerance {
inline constexpr AttributeId Id = 0x00000013;
} // namespace ScaledTolerance

namespace Scale {
inline constexpr AttributeId Id = 0x00000014;
} // namespace Scale

namespace GeneratedCommandList {
inline constexpr AttributeId Id = Globals::Attributes::GeneratedCommandList::Id;
} // namespace GeneratedCommandList

namespace AcceptedCommandList {
inline constexpr AttributeId Id = Globals::Attributes::AcceptedCommandList::Id;
} // namespace AcceptedCommandList

namespace AttributeList {
inline constexpr AttributeId Id = Globals::Attributes::AttributeList::Id;
} // namespace AttributeList

namespace FeatureMap {
inline constexpr AttributeId Id = Globals::Attributes::FeatureMap::Id;
} // namespace FeatureMap

namespace ClusterRevision {
inline constexpr AttributeId Id = Globals::Attributes::ClusterRevision::Id;
} // namespace ClusterRevision

} // namespace Attributes
} // namespace PressureMeasurement
} // namespace Clusters
} // namespace app
} // namespace chip
