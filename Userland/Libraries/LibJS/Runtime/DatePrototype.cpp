/*
 * Copyright (c) 2020-2021, Linus Groh <linusg@serenityos.org>
 * Copyright (c) 2021, Petróczi Zoltán <petroczizoltan@tutanota.com>
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <AK/Function.h>
#include <AK/String.h>
#include <AK/TypeCasts.h>
#include <LibCore/DateTime.h>
#include <LibCrypto/BigInt/UnsignedBigInteger.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/BigInt.h>
#include <LibJS/Runtime/Date.h>
#include <LibJS/Runtime/DatePrototype.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Intl/DateTimeFormat.h>
#include <LibJS/Runtime/Intl/DateTimeFormatConstructor.h>
#include <LibJS/Runtime/MarkedValueList.h>
#include <LibJS/Runtime/Temporal/Instant.h>
#include <LibJS/Runtime/Value.h>
#include <LibTimeZone/TimeZone.h>
#include <LibUnicode/DateTimeFormat.h>
#include <LibUnicode/Locale.h>

namespace JS {

// Table 62: Names of days of the week, https://tc39.es/ecma262/#sec-todatestring-day-names
static constexpr auto day_names = AK::Array { "Sun"sv, "Mon"sv, "Tue"sv, "Wed"sv, "Thu"sv, "Fri"sv, "Sat"sv };

// Table 63: Names of months of the year, https://tc39.es/ecma262/#sec-todatestring-day-names
static constexpr auto month_names = AK::Array { "Jan"sv, "Feb"sv, "Mar"sv, "Apr"sv, "May"sv, "Jun"sv, "Jul"sv, "Aug"sv, "Sep"sv, "Oct"sv, "Nov"sv, "Dec"sv };

DatePrototype::DatePrototype(GlobalObject& global_object)
    : PrototypeObject(*global_object.object_prototype())
{
}

void DatePrototype::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);
    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(vm.names.getDate, get_date, 0, attr);
    define_native_function(vm.names.getDay, get_day, 0, attr);
    define_native_function(vm.names.getFullYear, get_full_year, 0, attr);
    define_native_function(vm.names.getHours, get_hours, 0, attr);
    define_native_function(vm.names.getMilliseconds, get_milliseconds, 0, attr);
    define_native_function(vm.names.getMinutes, get_minutes, 0, attr);
    define_native_function(vm.names.getMonth, get_month, 0, attr);
    define_native_function(vm.names.getSeconds, get_seconds, 0, attr);
    define_native_function(vm.names.getTime, get_time, 0, attr);
    define_native_function(vm.names.getTimezoneOffset, get_timezone_offset, 0, attr);
    define_native_function(vm.names.getUTCDate, get_utc_date, 0, attr);
    define_native_function(vm.names.getUTCDay, get_utc_day, 0, attr);
    define_native_function(vm.names.getUTCFullYear, get_utc_full_year, 0, attr);
    define_native_function(vm.names.getUTCHours, get_utc_hours, 0, attr);
    define_native_function(vm.names.getUTCMilliseconds, get_utc_milliseconds, 0, attr);
    define_native_function(vm.names.getUTCMinutes, get_utc_minutes, 0, attr);
    define_native_function(vm.names.getUTCMonth, get_utc_month, 0, attr);
    define_native_function(vm.names.getUTCSeconds, get_utc_seconds, 0, attr);
    define_native_function(vm.names.setDate, set_date, 1, attr);
    define_native_function(vm.names.setFullYear, set_full_year, 3, attr);
    define_native_function(vm.names.setHours, set_hours, 4, attr);
    define_native_function(vm.names.setMilliseconds, set_milliseconds, 1, attr);
    define_native_function(vm.names.setMinutes, set_minutes, 3, attr);
    define_native_function(vm.names.setMonth, set_month, 2, attr);
    define_native_function(vm.names.setSeconds, set_seconds, 2, attr);
    define_native_function(vm.names.setTime, set_time, 1, attr);
    define_native_function(vm.names.setUTCDate, set_date, 1, attr);                 // FIXME: This is a hack, Serenity doesn't currently support timezones other than UTC.
    define_native_function(vm.names.setUTCFullYear, set_full_year, 3, attr);        // FIXME: see above
    define_native_function(vm.names.setUTCHours, set_hours, 4, attr);               // FIXME: see above
    define_native_function(vm.names.setUTCMilliseconds, set_milliseconds, 1, attr); // FIXME: see above
    define_native_function(vm.names.setUTCMinutes, set_minutes, 3, attr);           // FIXME: see above
    define_native_function(vm.names.setUTCMonth, set_month, 2, attr);               // FIXME: see above
    define_native_function(vm.names.setUTCSeconds, set_seconds, 2, attr);           // FIXME: see above
    define_native_function(vm.names.toDateString, to_date_string, 0, attr);
    define_native_function(vm.names.toISOString, to_iso_string, 0, attr);
    define_native_function(vm.names.toJSON, to_json, 1, attr);
    define_native_function(vm.names.toLocaleDateString, to_locale_date_string, 0, attr);
    define_native_function(vm.names.toLocaleString, to_locale_string, 0, attr);
    define_native_function(vm.names.toLocaleTimeString, to_locale_time_string, 0, attr);
    define_native_function(vm.names.toString, to_string, 0, attr);
    define_native_function(vm.names.toTemporalInstant, to_temporal_instant, 0, attr);
    define_native_function(vm.names.toTimeString, to_time_string, 0, attr);
    define_native_function(vm.names.toUTCString, to_utc_string, 0, attr);

    define_native_function(vm.names.getYear, get_year, 0, attr);
    define_native_function(vm.names.setYear, set_year, 1, attr);

    // 21.4.4.45 Date.prototype [ @@toPrimitive ] ( hint ), https://tc39.es/ecma262/#sec-date.prototype-@@toprimitive
    define_native_function(*vm.well_known_symbol_to_primitive(), symbol_to_primitive, 1, Attribute::Configurable);

    // Aliases.
    define_native_function(vm.names.valueOf, get_time, 0, attr);

    // B.2.4.3 Date.prototype.toGMTString ( ), https://tc39.es/ecma262/#sec-date.prototype.togmtstring
    // The function object that is the initial value of Date.prototype.toGMTString
    // is the same function object that is the initial value of Date.prototype.toUTCString.
    define_direct_property(vm.names.toGMTString, get_without_side_effects(vm.names.toUTCString), attr);
}

DatePrototype::~DatePrototype()
{
}

// thisTimeValue ( value ), https://tc39.es/ecma262/#thistimevalue
static ThrowCompletionOr<Value> this_time_value(GlobalObject& global_object, Value value)
{
    // 1. If Type(value) is Object and value has a [[DateValue]] internal slot, then
    if (value.is_object() && is<Date>(value.as_object())) {
        // a. Return value.[[DateValue]].
        return Value(static_cast<Date&>(value.as_object()).date_value());
    }

    // 2. Throw a TypeError exception.
    auto& vm = global_object.vm();
    return vm.throw_completion<TypeError>(global_object, ErrorType::NotAnObjectOfType, "Date");
}

// 21.4.4.2 Date.prototype.getDate ( ), https://tc39.es/ecma262/#sec-date.prototype.getdate
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_date)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->date());
}

// 21.4.4.3 Date.prototype.getDay ( ), https://tc39.es/ecma262/#sec-date.prototype.getday
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_day)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->day());
}

// 21.4.4.4 Date.prototype.getFullYear ( ), https://tc39.es/ecma262/#sec-date.prototype.getfullyear
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_full_year)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->year());
}

// 21.4.4.5 Date.prototype.getHours ( ), https://tc39.es/ecma262/#sec-date.prototype.gethours
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_hours)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->hours());
}

// 21.4.4.6 Date.prototype.getMilliseconds ( ), https://tc39.es/ecma262/#sec-date.prototype.getmilliseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_milliseconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->milliseconds());
}

// 21.4.4.7 Date.prototype.getMinutes ( ), https://tc39.es/ecma262/#sec-date.prototype.getminutes
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_minutes)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->minutes());
}

// 21.4.4.8 Date.prototype.getMonth ( ), https://tc39.es/ecma262/#sec-date.prototype.getmonth
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_month)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->month());
}

// 21.4.4.9 Date.prototype.getSeconds ( ), https://tc39.es/ecma262/#sec-date.prototype.getseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_seconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->seconds());
}

// 21.4.4.10 Date.prototype.getTime ( ), https://tc39.es/ecma262/#sec-date.prototype.gettime
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_time)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->time());
}

// 21.4.4.11 Date.prototype.getTimezoneOffset ( ), https://tc39.es/ecma262/#sec-date.prototype.gettimezoneoffset
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_timezone_offset)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    // FIXME: Make this actually do something once we support timezones instead of just UTC
    return Value(0);
}

// 21.4.4.12 Date.prototype.getUTCDate ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcdate
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_date)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_date());
}

// 21.4.4.13 Date.prototype.getUTCDay ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcday
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_day)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_day());
}

// 21.4.4.14 Date.prototype.getUTCFullYear ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcfullyear
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_full_year)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_full_year());
}

// 21.4.4.15 Date.prototype.getUTCHours ( ), https://tc39.es/ecma262/#sec-date.prototype.getutchours
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_hours)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_hours());
}

// 21.4.4.16 Date.prototype.getUTCMilliseconds ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcmilliseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_milliseconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_milliseconds());
}

// 21.4.4.17 Date.prototype.getUTCMinutes ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcminutes
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_minutes)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_minutes());
}

// 21.4.4.18 Date.prototype.getUTCMonth ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcmonth
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_month)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_month());
}

// 21.4.4.19 Date.prototype.getUTCSeconds ( ), https://tc39.es/ecma262/#sec-date.prototype.getutcseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_utc_seconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->utc_seconds());
}

// 21.4.4.20 Date.prototype.setDate ( date ), https://tc39.es/ecma262/#sec-date.prototype.setdate
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_date)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto& datetime = this_object->datetime();

    auto new_date_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_date_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_date = new_date_value.as_i32();

    datetime.set_time(datetime.year(), datetime.month(), new_date, datetime.hour(), datetime.minute(), datetime.second());
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.21 Date.prototype.setFullYear ( year [ , month [ , date ] ] ), https://tc39.es/ecma262/#sec-date.prototype.setfullyear
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_full_year)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto& datetime = this_object->datetime();

    auto arg_or = [&vm, &global_object](size_t i, i32 fallback) -> ThrowCompletionOr<Value> {
        return vm.argument_count() > i ? vm.argument(i).to_number(global_object) : Value(fallback);
    };

    auto new_year_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_year_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_year = new_year_value.as_i32();

    auto new_month_value = TRY(arg_or(1, datetime.month() - 1));
    if (!new_month_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_month = new_month_value.as_i32() + 1; // JS Months: 0 - 11, DateTime months: 1-12

    auto new_day_value = TRY(arg_or(2, datetime.day()));
    if (!new_day_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_day = new_day_value.as_i32();

    datetime.set_time(new_year, new_month, new_day, datetime.hour(), datetime.minute(), datetime.second());
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);

    return Value(this_object->time());
}

// 21.4.4.22 Date.prototype.setHours ( hour [ , min [ , sec [ , ms ] ] ] ), https://tc39.es/ecma262/#sec-date.prototype.sethours
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_hours)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto arg_or = [&vm, &global_object](size_t i, i32 fallback) -> ThrowCompletionOr<Value> {
        return vm.argument_count() > i ? vm.argument(i).to_number(global_object) : Value(fallback);
    };

    auto& datetime = this_object->datetime();

    auto new_hours_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_hours_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_hours = new_hours_value.as_i32();

    auto new_minutes_value = TRY(arg_or(1, datetime.minute()));
    if (!new_minutes_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_minutes = new_minutes_value.as_i32();

    auto new_seconds_value = TRY(arg_or(2, datetime.second()));
    if (!new_seconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_seconds = new_seconds_value.as_i32();

    auto new_milliseconds_value = TRY(arg_or(3, this_object->milliseconds()));
    if (!new_milliseconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_milliseconds = new_milliseconds_value.as_i32();

    new_seconds += new_milliseconds / 1000;
    this_object->set_milliseconds(new_milliseconds % 1000);

    datetime.set_time(datetime.year(), datetime.month(), datetime.day(), new_hours, new_minutes, new_seconds);
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.23 Date.prototype.setMilliseconds ( ms ), https://tc39.es/ecma262/#sec-date.prototype.setmilliseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_milliseconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto new_milliseconds_value = TRY(vm.argument(0).to_number(global_object));

    if (!new_milliseconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    auto new_milliseconds = new_milliseconds_value.as_i32();

    this_object->set_milliseconds(new_milliseconds % 1000);

    auto added_seconds = new_milliseconds / 1000;
    if (added_seconds > 0) {
        auto& datetime = this_object->datetime();
        datetime.set_time(datetime.year(), datetime.month(), datetime.day(), datetime.hour(), datetime.minute(), datetime.second() + added_seconds);
    }

    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);

    return Value(this_object->time());
}

// 21.4.4.24 Date.prototype.setMinutes ( min [ , sec [ , ms ] ] ), https://tc39.es/ecma262/#sec-date.prototype.setminutes
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_minutes)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto arg_or = [&vm, &global_object](size_t i, i32 fallback) -> ThrowCompletionOr<Value> {
        return vm.argument_count() > i ? vm.argument(i).to_number(global_object) : Value(fallback);
    };

    auto& datetime = this_object->datetime();

    auto new_minutes_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_minutes_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_minutes = new_minutes_value.as_i32();

    auto new_seconds_value = TRY(arg_or(1, datetime.second()));
    if (!new_seconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_seconds = new_seconds_value.as_i32();

    auto new_milliseconds_value = TRY(arg_or(2, this_object->milliseconds()));
    if (!new_milliseconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_milliseconds = new_milliseconds_value.as_i32();

    new_seconds += new_milliseconds / 1000;
    this_object->set_milliseconds(new_milliseconds % 1000);

    datetime.set_time(datetime.year(), datetime.month(), datetime.day(), datetime.hour(), new_minutes, new_seconds);
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.25 Date.prototype.setMonth ( month [ , date ] ), https://tc39.es/ecma262/#sec-date.prototype.setmonth
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_month)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto arg_or = [&vm, &global_object](size_t i, i32 fallback) -> ThrowCompletionOr<Value> {
        return vm.argument_count() > i ? vm.argument(i).to_number(global_object) : Value(fallback);
    };

    auto& datetime = this_object->datetime();

    auto new_month_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_month_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_month = new_month_value.as_i32() + 1; // JS Months: 0 - 11, DateTime months: 1-12

    auto new_date_value = TRY(arg_or(1, this_object->date()));
    if (!new_date_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_date = new_date_value.as_i32();

    datetime.set_time(datetime.year(), new_month, new_date, datetime.hour(), datetime.minute(), datetime.second());
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.26 Date.prototype.setSeconds ( sec [ , ms ] ), https://tc39.es/ecma262/#sec-date.prototype.setseconds
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_seconds)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto arg_or = [&vm, &global_object](size_t i, i32 fallback) -> ThrowCompletionOr<Value> {
        return vm.argument_count() > i ? vm.argument(i).to_number(global_object) : Value(fallback);
    };

    auto& datetime = this_object->datetime();

    auto new_seconds_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_seconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_seconds = new_seconds_value.as_i32();

    auto new_milliseconds_value = TRY(arg_or(1, this_object->milliseconds()));
    if (!new_milliseconds_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_milliseconds = new_milliseconds_value.as_i32();

    new_seconds += new_milliseconds / 1000;
    this_object->set_milliseconds(new_milliseconds % 1000);

    datetime.set_time(datetime.year(), datetime.month(), datetime.day(), datetime.hour(), datetime.minute(), new_seconds);
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.27 Date.prototype.setTime ( time ), https://tc39.es/ecma262/#sec-date.prototype.settime
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_time)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto new_time_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_time_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_time = new_time_value.as_double();

    if (new_time > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    auto new_date_time = Core::DateTime::from_timestamp(static_cast<time_t>(new_time / 1000));
    this_object->datetime().set_time(new_date_time.year(), new_date_time.month(), new_date_time.day(), new_date_time.hour(), new_date_time.minute(), new_date_time.second());
    this_object->set_milliseconds(static_cast<i16>(fmod(new_time, 1000)));

    this_object->set_is_invalid(false);
    return Value(this_object->time());
}

// 21.4.4.35 Date.prototype.toDateString ( ), https://tc39.es/ecma262/#sec-date.prototype.todatestring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_date_string)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_string(vm, "Invalid Date");

    auto string = this_object->date_string();
    return js_string(vm, move(string));
}

// 21.4.4.36 Date.prototype.toISOString ( ), https://tc39.es/ecma262/#sec-date.prototype.toisostring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_iso_string)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return vm.throw_completion<RangeError>(global_object, ErrorType::InvalidTimeValue);

    auto string = this_object->iso_date_string();
    return js_string(vm, move(string));
}

// 21.4.4.37 Date.prototype.toJSON ( key ), https://tc39.es/ecma262/#sec-date.prototype.tojson
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_json)
{
    auto this_value = vm.this_value(global_object);

    auto time_value = TRY(this_value.to_primitive(global_object, Value::PreferredType::Number));

    if (time_value.is_number() && !time_value.is_finite_number())
        return js_null();

    return TRY(this_value.invoke(global_object, vm.names.toISOString));
}

static ThrowCompletionOr<Intl::DateTimeFormat*> construct_date_time_format(GlobalObject& global_object, Value locales, Value options)
{
    MarkedValueList arguments { global_object.vm().heap() };
    arguments.append(locales);
    arguments.append(options);

    auto* date_time_format = TRY(construct(global_object, *global_object.intl_date_time_format_constructor(), move(arguments)));
    return static_cast<Intl::DateTimeFormat*>(date_time_format);
}

// 21.4.4.38 Date.prototype.toLocaleDateString ( [ reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-date.prototype.tolocaledatestring
// 18.4.2 Date.prototype.toLocaleDateString ( [ locales [ , options ] ] ), https://tc39.es/ecma402/#sup-date.prototype.tolocaledatestring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_locale_date_string)
{
    auto locales = vm.argument(0);
    auto options = vm.argument(1);

    // 1. Let x be ? thisTimeValue(this value).
    auto time = TRY(this_time_value(global_object, vm.this_value(global_object)));

    // 2. If x is NaN, return "Invalid Date".
    if (time.is_nan())
        return js_string(vm, "Invalid Date"sv);

    // 3. Let options be ? ToDateTimeOptions(options, "date", "date").
    options = Value(TRY(Intl::to_date_time_options(global_object, options, Intl::OptionRequired::Date, Intl::OptionDefaults::Date)));

    // 4. Let dateFormat be ? Construct(%DateTimeFormat%, « locales, options »).
    auto* date_format = TRY(construct_date_time_format(global_object, locales, options));

    // 5. Return ? FormatDateTime(dateFormat, x).
    auto formatted = TRY(Intl::format_date_time(global_object, *date_format, time));
    return js_string(vm, move(formatted));
}

// 21.4.4.39 Date.prototype.toLocaleString ( [ reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-date.prototype.tolocalestring
// 18.4.1 Date.prototype.toLocaleString ( [ locales [ , options ] ] ), https://tc39.es/ecma402/#sup-date.prototype.tolocalestring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_locale_string)
{
    auto locales = vm.argument(0);
    auto options = vm.argument(1);

    // 1. Let x be ? thisTimeValue(this value).
    auto time = TRY(this_time_value(global_object, vm.this_value(global_object)));

    // 2. If x is NaN, return "Invalid Date".
    if (time.is_nan())
        return js_string(vm, "Invalid Date"sv);

    // 3. Let options be ? ToDateTimeOptions(options, "any", "all").
    options = Value(TRY(Intl::to_date_time_options(global_object, options, Intl::OptionRequired::Any, Intl::OptionDefaults::All)));

    // 4. Let dateFormat be ? Construct(%DateTimeFormat%, « locales, options »).
    auto* date_format = TRY(construct_date_time_format(global_object, locales, options));

    // 5. Return ? FormatDateTime(dateFormat, x).
    auto formatted = TRY(Intl::format_date_time(global_object, *date_format, time));
    return js_string(vm, move(formatted));
}

// 21.4.4.40 Date.prototype.toLocaleTimeString ( [ reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-date.prototype.tolocaletimestring
// 18.4.3 Date.prototype.toLocaleTimeString ( [ locales [ , options ] ] ), https://tc39.es/ecma402/#sup-date.prototype.tolocaletimestring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_locale_time_string)
{
    auto locales = vm.argument(0);
    auto options = vm.argument(1);

    // 1. Let x be ? thisTimeValue(this value).
    auto time = TRY(this_time_value(global_object, vm.this_value(global_object)));

    // 2. If x is NaN, return "Invalid Date".
    if (time.is_nan())
        return js_string(vm, "Invalid Date"sv);

    // 3. Let options be ? ToDateTimeOptions(options, "time", "time").
    options = Value(TRY(Intl::to_date_time_options(global_object, options, Intl::OptionRequired::Time, Intl::OptionDefaults::Time)));

    // 4. Let timeFormat be ? Construct(%DateTimeFormat%, « locales, options »).
    auto* time_format = TRY(construct_date_time_format(global_object, locales, options));

    // 5. Return ? FormatDateTime(dateFormat, x).
    auto formatted = TRY(Intl::format_date_time(global_object, *time_format, time));
    return js_string(vm, move(formatted));
}

// 21.4.4.41 Date.prototype.toString ( ), https://tc39.es/ecma262/#sec-date.prototype.tostring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_string)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_string(vm, "Invalid Date");

    auto string = this_object->string();
    return js_string(vm, move(string));
}

// 21.4.4.41.1 TimeString ( tv ), https://tc39.es/ecma262/#sec-timestring
String time_string(double time)
{
    // 1. Let hour be the String representation of HourFromTime(tv), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto hour = hour_from_time(time);

    // 2. Let minute be the String representation of MinFromTime(tv), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto minute = min_from_time(time);

    // 3. Let second be the String representation of SecFromTime(tv), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto second = sec_from_time(time);

    // 4. Return the string-concatenation of hour, ":", minute, ":", second, the code unit 0x0020 (SPACE), and "GMT".
    return String::formatted("{:02}:{:02}:{:02} GMT", hour, minute, second);
}

// 21.4.4.41.2 DateString ( tv ), https://tc39.es/ecma262/#sec-datestring
String date_string(double time)
{
    // 1. Let weekday be the Name of the entry in Table 62 with the Number WeekDay(tv).
    auto weekday = day_names[week_day(time)];

    // 2. Let month be the Name of the entry in Table 63 with the Number MonthFromTime(tv).
    auto month = month_names[month_from_time(time)];

    // 3. Let day be the String representation of DateFromTime(tv), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto day = date_from_time(time);

    // 4. Let yv be YearFromTime(tv).
    auto year = year_from_time(time);

    // 5. If yv ≥ +0𝔽, let yearSign be the empty String; otherwise, let yearSign be "-".
    auto year_sign = year >= 0 ? ""sv : "-"sv;

    // 6. Let year be the String representation of abs(ℝ(yv)), formatted as a decimal number.
    year = abs(year);

    // 7. Let paddedYear be ! StringPad(year, 4𝔽, "0", start).
    // 8. Return the string-concatenation of weekday, the code unit 0x0020 (SPACE), month, the code unit 0x0020 (SPACE), day, the code unit 0x0020 (SPACE), yearSign, and paddedYear.
    return String::formatted("{} {} {:02} {}{:04}", weekday, month, day, year_sign, year);
}

// 21.4.4.41.3 TimeZoneString ( tv ), https://tc39.es/ecma262/#sec-timezoneestring
String time_zone_string(double time)
{
    // 1. Let offset be LocalTZA(tv, true).
    auto offset = local_tza(time, true);

    StringView offset_sign;

    // 2. If offset ≥ +0𝔽, then
    if (offset >= 0) {
        // a. Let offsetSign be "+".
        offset_sign = "+"sv;
        // b. Let absOffset be offset.
    }
    // 3. Else,
    else {
        // a. Let offsetSign be "-".
        offset_sign = "-"sv;
        // b. Let absOffset be -offset.
        offset *= -1;
    }

    // 4. Let offsetMin be the String representation of MinFromTime(absOffset), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto offset_min = min_from_time(offset);

    // 5. Let offsetHour be the String representation of HourFromTime(absOffset), formatted as a two-digit decimal number, padded to the left with the code unit 0x0030 (DIGIT ZERO) if necessary.
    auto offset_hour = hour_from_time(offset);

    // 6. Let tzName be an implementation-defined string that is either the empty String or the string-concatenation of the code unit 0x0020 (SPACE), the code unit 0x0028 (LEFT PARENTHESIS), an implementation-defined timezone name, and the code unit 0x0029 (RIGHT PARENTHESIS).
    auto tz_name = TimeZone::current_time_zone();

    // Most implementations seem to prefer the long-form display name of the time zone. Not super important, but we may as well match that behavior.
    if (auto long_name = Unicode::get_time_zone_name(Unicode::default_locale(), tz_name, Unicode::CalendarPatternStyle::Long); long_name.has_value())
        tz_name = long_name.release_value();

    // 7. Return the string-concatenation of offsetSign, offsetHour, offsetMin, and tzName.
    return String::formatted("{}{:02}{:02} ({})", offset_sign, offset_hour, offset_min, tz_name);
}

// 21.4.4.41.4 ToDateString ( tv ), https://tc39.es/ecma262/#sec-todatestring
String to_date_string(double time)
{
    // 1. If tv is NaN, return "Invalid Date".
    if (Value(time).is_nan())
        return "Invalid Date"sv;

    // 2. Let t be LocalTime(tv).
    time = local_time(time);

    // 3. Return the string-concatenation of DateString(t), the code unit 0x0020 (SPACE), TimeString(t), and TimeZoneString(tv).
    return String::formatted("{} {}{}", date_string(time), time_string(time), time_zone_string(time));
}

// 14.1.1 Date.prototype.toTemporalInstant ( ), https://tc39.es/proposal-temporal/#sec-date.prototype.totemporalinstant
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_temporal_instant)
{
    // 1. Let t be ? thisTimeValue(this value).
    auto t = TRY(this_time_value(global_object, vm.this_value(global_object)));

    // 2. Let ns be ? NumberToBigInt(t) × 10^6.
    auto* ns = TRY(number_to_bigint(global_object, t));
    ns = js_bigint(vm, ns->big_integer().multiplied_by(Crypto::UnsignedBigInteger { 1'000'000 }));

    // 3. Return ! CreateTemporalInstant(ns).
    return MUST(Temporal::create_temporal_instant(global_object, *ns));
}

// 21.4.4.42 Date.prototype.toTimeString ( ), https://tc39.es/ecma262/#sec-date.prototype.totimestring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_time_string)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_string(vm, "Invalid Date");

    auto string = this_object->time_string();
    return js_string(vm, move(string));
}

// 21.4.4.43 Date.prototype.toUTCString ( ), https://tc39.es/ecma262/#sec-date.prototype.toutcstring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_utc_string)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_string(vm, "Invalid Date");

    // HTTP dates are always expressed in GMT.
    auto string = this_object->gmt_date_string();
    return js_string(vm, move(string));
}

// 21.4.4.45 Date.prototype [ @@toPrimitive ] ( hint ), https://tc39.es/ecma262/#sec-date.prototype-@@toprimitive
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::symbol_to_primitive)
{
    auto this_value = vm.this_value(global_object);
    if (!this_value.is_object())
        return vm.throw_completion<TypeError>(global_object, ErrorType::NotAnObject, this_value.to_string_without_side_effects());
    auto hint_value = vm.argument(0);
    if (!hint_value.is_string())
        return vm.throw_completion<TypeError>(global_object, ErrorType::InvalidHint, hint_value.to_string_without_side_effects());
    auto& hint = hint_value.as_string().string();
    Value::PreferredType try_first;
    if (hint == "string" || hint == "default")
        try_first = Value::PreferredType::String;
    else if (hint == "number")
        try_first = Value::PreferredType::Number;
    else
        return vm.throw_completion<TypeError>(global_object, ErrorType::InvalidHint, hint);
    return TRY(this_value.as_object().ordinary_to_primitive(try_first));
}

// B.2.4.1 Date.prototype.getYear ( ), https://tc39.es/ecma262/#sec-date.prototype.getyear
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::get_year)
{
    auto* this_object = TRY(typed_this_object(global_object));

    if (this_object->is_invalid())
        return js_nan();

    return Value(this_object->year() - 1900);
}

// B.2.4.2 Date.prototype.setYear ( year ), https://tc39.es/ecma262/#sec-date.prototype.setyear
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::set_year)
{
    auto* this_object = TRY(typed_this_object(global_object));

    auto& datetime = this_object->datetime();

    auto new_year_value = TRY(vm.argument(0).to_number(global_object));
    if (!new_year_value.is_finite_number()) {
        this_object->set_is_invalid(true);
        return js_nan();
    }
    auto new_year = new_year_value.as_i32();
    if (new_year >= 0 && new_year <= 99)
        new_year += 1900;

    datetime.set_time(new_year, datetime.month(), datetime.day(), datetime.hour(), datetime.minute(), datetime.second());
    if (this_object->time() > Date::time_clip) {
        this_object->set_is_invalid(true);
        return js_nan();
    }

    this_object->set_is_invalid(false);

    return Value(this_object->time());
}

// B.2.4.3 Date.prototype.toGMTString ( ), https://tc39.es/ecma262/#sec-date.prototype.togmtstring
JS_DEFINE_NATIVE_FUNCTION(DatePrototype::to_gmt_string)
{
    // NOTE: The toUTCString method is preferred. The toGMTString method is provided principally for compatibility with old code.
    return to_utc_string(vm, global_object);
}

}
