#pragma once

#include "./control_param.hpp"

#ifndef AIP_DISABLE_MACROS
/**
 * @def AIP_DEFINE_CONTROL_PARAM(Type, Name)
 * @brief Объявляет управляемый параметр с именем, равным идентификатору переменной.
 *
 * Макрос создаёт переменную с именем @p Name типа
 * `aip::params::ControlParam<Type, aip::core::fixed_string{#Name}>`.
 *
 * То есть имя параметра фиксируется на этапе компиляции как строка, полученная из
 * идентификатора переменной.
 *
 * @param Type Тип значения параметра (например, double).
 * @param Name Идентификатор переменной (например, k).
 *
 * @warning Это макрос: он попадает в глобальное пространство имён препроцессора.
 *          Рекомендуется подключать заголовок с макросами только там, где он нужен.
 */
#define AIP_DEFINE_CONTROL_PARAM(Type, Name) \
    aip::params::ControlParam<Type, aip::core::fixed_string{#Name}> Name {}
#endif