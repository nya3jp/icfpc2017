#ifndef PUNTER_PUNTER_FACTORY_H__
#define PUNTER_PUNTER_FACTORY_H__

#include <memory>
#include <string>
#include "framework/game.h"

namespace punter {
std::unique_ptr<framework::Punter> PunterByName(const std::string& name);
}  // namespace punter

#endif  // PUNTER_PUNTER_FACTORY_H__
