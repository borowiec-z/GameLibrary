#include "GameLibrary/Console/Console.h"

#include <string>
#include <vector>

#include "catch2/catch.hpp"

#include "GameLibrary/Exceptions/Standard.h"

using namespace GameLibrary::Exceptions;
using namespace GameLibrary::Console;

/*
 *  Notes:
 *    - Some tests for registering / dispatching Commands are not written, because it works exactly like already tested Cvar dispatching.
 *	  - Maybe rewrite this convoluted file.
 */


TEST_CASE("Console adds and removes ConsoleObject-deriving classes.")
{
	static int latestMyClassConstructionStatus = 0;
	static int latestMyClassDestructionStatus = 0;

	struct MyClass : public ConsoleObject {
		MyClass(Console& c, Id id, int constructionStatus, int destructionStatus) : ConsoleObject(c, id), _destructionStatus(destructionStatus) {
			latestMyClassConstructionStatus = constructionStatus;
		}
		~MyClass()  {
			latestMyClassDestructionStatus = _destructionStatus;
		}

		const int _destructionStatus;
	};

	Console c;

	const auto firstObjectId = c.addObject<MyClass>(1, 10);
	REQUIRE(latestMyClassConstructionStatus == 1);

	const auto secondObjectId = c.addObject<MyClass>(2, 20);
	REQUIRE(latestMyClassConstructionStatus == 2);

	const auto thirdObjectId = c.addObject<MyClass>(3, 30);
	REQUIRE(latestMyClassConstructionStatus == 3);


	c.removeObject(firstObjectId);
	REQUIRE(latestMyClassDestructionStatus == 10);

	c.removeObject(thirdObjectId);
	REQUIRE(latestMyClassDestructionStatus == 30);

	c.removeObject(secondObjectId);
	REQUIRE(latestMyClassDestructionStatus == 20);
}

TEST_CASE("Console initializes Cvars returned by T::getCvars(), and throws on invalid getCvar().")
{
	struct CvarHolder1 {
		static CvarCollection getCvars() {
			CvarCollection ret;

			ret.emplace_back("sv_cheats", Cvar::ValueType::Integer, 0);
			ret.emplace_back("volume", Cvar::ValueType::Float, 1.5);

			return ret;
		};
	};

	struct CvarHolder2 {
		static CvarCollection getCvars() {
			CvarCollection ret;

			ret.emplace_back("name", Cvar::ValueType::String, "default_name");
			ret.emplace_back("group", Cvar::ValueType::String, "default_group");

			return ret;
		}
	};

	Console c;
	c.initCvars<CvarHolder1, CvarHolder2>();

	REQUIRE(c.getCvar("sv_cheats").getAs<int>() == 0);
	REQUIRE(c.getCvar("volume").getAs<float>() == Approx(1.5));
	REQUIRE(c.getCvar("name").getAsString() == "default_name");
	REQUIRE(c.getCvar("group").getAsString() == "default_group");

	REQUIRE_THROWS_AS(c.getCvar("invalid").getAsString(), NotFoundError);
	REQUIRE_THROWS_AS(c.getCvar("invalid"), NotFoundError);
}

TEST_CASE("Console sets Cvar values using setCvar() or parse().")
{
	struct CvarHolder {
		static CvarCollection getCvars() {
			CvarCollection ret;

			ret.emplace_back("sv_cheats", Cvar::ValueType::Integer);
			ret.emplace_back("volume", Cvar::ValueType::Float);
			ret.emplace_back("name", Cvar::ValueType::String);

			return ret;
		}
	};

	Console c;
	c.initCvars<CvarHolder>();

	c.setCvar("sv_cheats", 10);
	c.parse("volume 0.55");
	c.parse("  name   Player of game  ");

	REQUIRE(c.getCvar("sv_cheats").getAs<int>() == 10);
	REQUIRE(c.getCvar("volume").getAs<float>() == Approx(0.55));
	REQUIRE(c.getCvar("name").getAsString() == "Player of game  ");

	// Console should simply ignore invalid arguments given to setters, and leave the value unchanged.
	REQUIRE_NOTHROW(c.setCvar("sv_cheats", "invalid"));
	REQUIRE_NOTHROW(c.parse("sv_cheats invalid"));
	REQUIRE(c.getCvar("sv_cheats").getAs<int>() == 10);

	// Setting non-existent Cvars should do nothing at the moment.
	REQUIRE_NOTHROW(c.setCvar("invalid", 100));
}

TEST_CASE("Console adds/removes Cvar listeners and calls them on Cvar setter calls.")
{
	Console c;

	struct CvarHolder {
		static CvarCollection getCvars() {
			CvarCollection ret;

			ret.emplace_back("volume", Cvar::ValueType::Float, 0);
			ret.emplace_back("name", Cvar::ValueType::String, "");

			return ret;
		};
	};

	c.initCvars<CvarHolder>();

	SECTION("Non-owned callbacks")
	{
		Float volumeAfterChange = 0;
		auto onVolumeChange = [ &volumeAfterChange ] ( const CvarValueChangedEvent& e ) {
			volumeAfterChange = e.cvar.getAs<Float>();
		};

		String nameAfterChange = "";
		auto onNameChange = [ &nameAfterChange ] ( const CvarValueChangedEvent& e ) {
			nameAfterChange = e.cvar.getAsString();
		};

		const auto key1 = c.addCvarListener("volume", onVolumeChange);
		const auto key2 = c.addCvarListener("name", onNameChange);

		c.setCvar("volume", 99.9);
		c.parse("name Player");

		REQUIRE(volumeAfterChange == Approx(99.9));
		REQUIRE(nameAfterChange == "Player");

		// Make sure callbacks don't get called after removing.
		volumeAfterChange = 0;
		nameAfterChange = "";

		c.removeListener(key1);
		c.removeListener(key2);

		c.parse("volume 1");
		c.setCvar("name", "AnotherName");

		REQUIRE(volumeAfterChange == 0);
		REQUIRE(nameAfterChange == "");
	}

	SECTION("Owned callbacks")
	{
		struct CallbackOwner : public ConsoleObject {
			CallbackOwner(Console& c, Id id) : ConsoleObject(c, id) {}
		};
		
		const auto objectWithTwoCallbacks = c.addObject<CallbackOwner>();
		const auto objectWithFiveCallbacks = c.addObject<CallbackOwner>();
		std::vector<GameLibrary::Event::Dispatcher::Key> keysOfFiveCallbacksOwner;

		int callCount = 0;
		auto incrementCallCount = [ &callCount ] { ++callCount; };
		
		for (int i = 0; i < 2; ++i)
			c.addOwnedCvarListener(objectWithTwoCallbacks, "volume", incrementCallCount);
		for (int i = 0; i < 5; ++i)
			keysOfFiveCallbacksOwner.emplace_back(c.addOwnedCvarListener(objectWithFiveCallbacks, "volume", incrementCallCount));

		// One object has 2 callback incrementing callCount, another object has 5.
		c.setCvar("volume", 1);
		REQUIRE(callCount == 7);

		// After removeObject(), and removal of one owned listener, only one object with four incrementers should remain.
		callCount = 0;
		c.removeObject(objectWithTwoCallbacks);
		c.removeOwnedListener(objectWithFiveCallbacks, keysOfFiveCallbacksOwner[0]);
		c.parse("volume 1.5");
		REQUIRE(callCount == 4);
	}

	SECTION("Member functions")
	{
		static int globalValue = 0;
		static double volumeLastValue = 0;

		// This class will tests two Cvar callback signatures: no parameters, and const Event& parameter.
		class VolumeListener : public ConsoleObject {
		public:
			VolumeListener(Console& c, Id id, int value) : ConsoleObject(c, id), _value(value) { }

			virtual void onCreation() override {
				addCvarListener("name", &VolumeListener::setGlobalValue);
				addCvarListener("volume", &VolumeListener::setVolumeValue);
			}

			void setGlobalValue() {
				globalValue = _value;
			}

			void setVolumeValue(const CvarValueChangedEvent& e) {
				volumeLastValue = e.cvar.getAs<double>();
			}

		private:
			int _value;
		};

		const auto objectId = c.addObject<VolumeListener>(10);
		
		// This should make object added in previous statement call its setGlobalValue(), setting globalValue to 10.
		c.setCvar("name", "Player.");
		REQUIRE(globalValue == 10);

		// VolumeListener sets volumeLastValue to Cvar's value on "volume" Cvar change.
		c.parse("volume 100");
		REQUIRE(volumeLastValue == Approx(100));


		// Make sure callbacks do not get called after object is removed.
		globalValue = 0;
		volumeLastValue = 0;

		c.removeObject(objectId);

		c.setCvar("name", "Player.");
		c.setCvar("volume", 100);

		REQUIRE(globalValue == 0);
		REQUIRE(volumeLastValue == 0);
	}
}

TEST_CASE("Console adds/removes Command listeners, and calls them when executing a registered Command with correct amount of arguments.")
{
	Console c;

	struct CommandHolder {
		static CommandInfoCollection getCommandInfos() {
			CommandInfoCollection ret;

			ret.emplace_back("set_the_variable", 1, "Sets variable handledCommandArgValue to argument.");
			ret.emplace_back("set_another_variable_to_sum", CommandInfo::ParamsCount::Any, "Sets variable handledAnotherCommandArgValue to sum of args.");

			return ret;
		}
	};

	c.initCommandInfos<CommandHolder>();
	
	// Two variables and commands just to ensure one dispatch doesn't call all callbacks, only those listening to sent name.
	int handledCommandArgValue = 0;
	int handledAnotherCommandArgValue = 0;

	const auto listenerKey = c.addCommandListener("set_the_variable", [ &handledCommandArgValue ] ( const CommandSentEvent& e ) {
		handledCommandArgValue = std::stoi(e.command.getArgs()[0]);
	});
	c.addCommandListener("set_another_variable_to_sum", [ &handledAnotherCommandArgValue ] ( const CommandSentEvent& e ) {
		int sum = 0;
		for (const auto& arg : e.command.getArgs())
			sum += std::stoi(arg);
		handledAnotherCommandArgValue = sum;
	});

	c.dispatchCommand(Command("set_the_variable 101"));
	c.parse("set_another_variable_to_sum 100 200 300 400");

	// This command will be ignored due to invalid amount of arguments.
	c.dispatchCommand(Command("set_the_variable 105 102 103"));

	REQUIRE(handledCommandArgValue == 101);
	REQUIRE(handledAnotherCommandArgValue == 1000);

	// Remove listener writing to handledCommandArgValue, make sure it stops getting called.
	// And make sure listener writing to handledAnotherCommandArgValue is unaffected.
	c.removeListener(listenerKey);

	handledCommandArgValue = 0;
	handledAnotherCommandArgValue = 0;

	c.dispatchCommand(Command("set_the_variable 101"));
	c.parse("set_another_variable_to_sum 101 101");

	REQUIRE(handledCommandArgValue == 0);
	REQUIRE(handledAnotherCommandArgValue == 202);
}

