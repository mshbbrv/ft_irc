#include "Server.hpp"

int Server::pass(User &user, Input &input)
{
	if (input.getParams().size() == 0) //если не указан пароль
		sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());
	else if (user.getFlags() & USER_REGISTERED) //зареган ли этот пользователь раньше
		sendServerReply(user, ERR_ALREADYREGISTRED);
	else
		user.setPassword(input.getParams()[0]);
	return (0);
}

int Server::nick(User &user, Input &input)
{
	if (input.getParams().size() == 0) //если не указан ник
		sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());
	else if (!isNickValid(input.getParams()[0]) || input.getParams()[0] == ircName) //Чек на символы
		sendServerReply(user, ERR_ERRONEUSNICKNAME, input.getParams()[0]);
	else if (containsNickname(input.getParams()[0])) //Чек на присутствие другого юзера с ником
	{
		if (user.getNick() == "")
			user.setNick("*");
		sendServerReply(user, ERR_NICKNAMEINUSE, input.getParams()[0]);
		if (user.getNick() == "*")
			user.setNick("");
	}
	else
	{
		if (user.getFlags() & USER_REGISTERED)
		{
			std::string message = ":" + user.getMask() + " NICK :" + input.getParams()[0];
			user.sendToAllUserChannels(message);
		}
		user.setNick(input.getParams()[0]);
	}
	return (checkForRegistration(user));
}

int Server::user(User &user, Input &input)
{
	if (input.getParams().size() < 4)
		sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());
	else if (user.getFlags() & USER_REGISTERED)
		sendServerReply(user, ERR_ALREADYREGISTRED);
	else
	{
		user.setUserName(input.getParams()[0]);
		user.setRealName(input.getParams()[3]);
	}
	return (checkForRegistration(user));
}

// Суть данного чека: Проверка пароля только если и ник и имя было указано.
// Если же пароль не верен (была или не была использована команда PASS - неважно), то клиент отключается
int Server::checkForRegistration(User &user)
{
	if (user.getNick().size() == 0 || user.getUserName().size() == 0) //Чек на наличие ника и имени
		return (0);
	if (!(_password.size() == 0 || user.getPassword() == _password)) //Чек на пароль
	{
		sendServerReply(user, ERR_PASSWDMISMATCH);
		user.setQuitMessage("Bad Password");
		return (SERVER_DISCONNECT);
	}

	if (!(user.getFlags() & USER_REGISTERED))
	{
		user.setFlags(USER_REGISTERED);
		sendWelcome(user);
		sendMOTD(user);
	}
	return (0);
}

void Server::sendWelcome(const User &user) const
{
	std::string version = "0.678";
	std::string date = "Today";
	sendServerReply(user, RPL_WELCOME, user.getNick(), user.getUserName(), user.getHostName());
	sendServerReply(user, RPL_YOURHOST, ircName, version);
	sendServerReply(user, RPL_CREATED, date);
	sendServerReply(user, RPL_MYINFO, ircName, version, "iosw", "biklmnopstv");
	user.sendMessage(":" + user.getMask() + " MODE " + user.getNick() + " +s");
}

void Server::sendMOTD(const User &user) const
{
	if (_motd.size() == 0)
		sendServerReply(user, ERR_NOMOTD);
	else
	{
		sendServerReply(user, RPL_MOTDSTART, ircName);
		for (size_t i = 0; i < _motd.size(); ++i)
			sendServerReply(user, RPL_MOTD, _motd[i]);
		sendServerReply(user, RPL_ENDOFMOTD);
	}
}

/*
 ****************************************************************************
 */

int Server::join(User &user, Input &input) {

	if (input.getParams().size() == 0)
        return sendServerReply(user, ERR_NEEDMOREPARAMS, "JOIN");

	std::queue<std::string> channels = User::split(input.getParams()[0], ',', 1);
	std::queue<std::string> keys;
	if (input.getParams().size() > 1)
		keys = User::split(input.getParams()[1], ',', 1);

	for (; !channels.empty(); channels.pop()) {

		std::string channelName = channels.front();
		std::string channelKey;
		if (!keys.empty()) {
			channelKey = keys.front();
			keys.pop();
		} else
			channelKey = "";

		if (!Channel::isChannelNameCorrect(channelName))
			sendServerReply(user, ERR_NOSUCHCHANNEL, channelName);
		else if (user.getChannels().size() >= 10)
			sendServerReply(user, ERR_TOOMANYCHANNELS, channelName);
		else {
			try {
				Channel *tmp = _channels.at(channelName);
				if (tmp->connect(user, channelKey) == 0)
					user.addNewChannel(*(_channels.at(channelName)));
			}
			catch (const std::exception &e) {
				_channels[channelName] = new Channel(channelName, user);
				user.addNewChannel(*(_channels.at(channelName)));
			}
		}
	}
	return 0;
}

int	Server::kick(User &user, Input &input)
{
	if (input.getParams().size() < 2)
		return sendServerReply(user, ERR_NEEDMOREPARAMS, "KICK");

	std::string channelName = input.getParams()[0];
	std::string nickName = input.getParams()[1];

	if (_channels.find(channelName) == _channels.end()) //нет указанного канала
		sendServerReply(user, ERR_NOSUCHCHANNEL, channelName);
	else if (!containsNickname(nickName)) //нет указанного ника
		sendServerReply(user, ERR_NOSUCHNICK, nickName);
	else if (!_channels.at(channelName)->isChannelUser(user.getNick())) //не на канале
		sendServerReply(user, ERR_NOTONCHANNEL, channelName);
	else if (!_channels.at(channelName)->isOperator(user)) //не оператор канала
		sendServerReply(user, ERR_CHANOPRIVSNEEDED, channelName);
	else if (!_channels.at(channelName)->isChannelUser(nickName))
		sendServerReply(user, ERR_USERNOTINCHANNEL, nickName, channelName);
	else {
		Channel *channel = _channels.at(channelName);
		std::string message = "KICK " + channel->getName() + " " + nickName + " ";

		if (input.getParams().size() > 2)
			message += ":" + input.getParams()[2];
		else
			message += ":no reason";

		std::cout << message << "\n";
		user.sendMessage(":" + user.getMask() + " " + message);
		channel->sendNotification(message, user);
		User *userToBeKicked;
		for (size_t i = 0; i < _users.size(); i++)
			if (_users[i]->getNick() == nickName)
				userToBeKicked = _users[i];
		channel->disconnect(*(userToBeKicked));
		userToBeKicked->leaveChannel(channelName);
	}
	return 0;
}

int Server::part(User &user, Input &input) {

	if (input.getParams().size() < 1)
		sendServerReply(user, ERR_NEEDMOREPARAMS, "PART");
	else {
		std::queue<std::string>	channels = User::split(input.getParams()[0], ',', false);
		while (!channels.empty())
		{
			if (!containsChannel(channels.front()))
				sendServerReply(user, ERR_NOSUCHCHANNEL, channels.front());
			else if (!user.isChannelMember(channels.front()))
				sendServerReply(user, ERR_NOTONCHANNEL, channels.front());
			else {
				user.sendMessage(":" + user.getMask() + " PART " + channels.front());
				_channels.at(channels.front())->sendNotification("PART " + channels.front(), user);
				_channels.at(channels.front())->disconnect(user);
				user.leaveChannel(channels.front());
			}
			channels.pop();
		}
	}
	return 0;
}

static bool checkModeFlags(std::string object, std::string &_flags)
{
	if (_flags[0] != '+' && _flags[0] != '-')
		_flags = "+" + _flags;
	const char *flags = _flags.c_str() + 1; // пропускаем +-
	const char *dict;
	if (_flags.size() == 1)
		return false;
	if (_flags.size() == 2 && (object[0] == '#' || object[0] == '&'))
		dict = "opsitnmlbvk";
	else if (object[0] == '#' || object[0] == '&')
		dict = "psitnm";
	else
		dict = "isow";
	// проверка на отличные символы в flags от символов в dict
	if (strspn(flags, dict) != _flags.size() - 1)
		return false;
	// проверка на повторы
	for (; *dict; dict++)
		if (_flags.find(*dict) != _flags.rfind(*dict))
			return false;
	return true;
}

int Server::mode(User &user, Input &input)
{
	if (input.getParams().size() < 1)
		return sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());

	if (input.getParams().size() < 2) //check MODE for user/channel
	{
		std::string target = input.getParams()[0];
		std::string keys = "+";
		if (target[0] == '#' || target[0] == '&') //channel
		{
			Channel *channel;
			try { channel = _channels.at(target); } //find channel, if not - error
			catch (const std::exception &e) { return sendServerReply(user, ERR_NOSUCHCHANNEL, target); }

			std::queue<std::string> argsToKeys;
			std::string args;
			unsigned char flag = channel->getFlags();
			//iklmnpst
			if (flag & CHL_INVITEONLY)
				keys += "i";
			if (flag & CHL_PASSWORDED)
			{
				keys += "k";
				argsToKeys.push(channel->getPass());
			}
			if (channel->getLimit() != -1)
			{
				keys += "l";
				std::stringstream ss;
				ss << channel->getLimit();
				argsToKeys.push(ss.str());
			}
			if (flag & CHL_MODERATED)
				keys += "m";
			if (flag & CHL_NOMSGOUT)
				keys += "n";
			if (flag & CHL_PRIVATE)
				keys += "p";
			if (flag & CHL_SECRET)
				keys += "s";
			if (flag & CHL_TOPICSET)
				keys += "t";

			while (argsToKeys.size())
			{
				if (args.empty())
					args = argsToKeys.front();
				else
					args += " " + argsToKeys.front();
				argsToKeys.pop();
			}
			sendServerReply(user, RPL_CHANNELMODEIS, target, keys, args);
		}
		else
		{
			if (user.getNick() != target)
				return sendServerReply(user, ERR_USERSDONTMATCH);
			User *tmpUser = this->searchUser(SRCH_NICK, target);
			if (!tmpUser)
				return sendServerReply(user, ERR_NOSUCHNICK, target);

			unsigned char flag = tmpUser->getFlags();
			//iosw
			if (flag & USER_INVISIBLE)
				keys += "i";
			if (flag & USER_OPERATOR)
				keys += "o";
			if (flag & USER_GETNOTICE)
				keys += "s";
			if (flag & USER_GETWALLOPS)
				keys += "w";
			
			sendServerReply(user, RPL_UMODEIS, keys);
		}
		return (0);
	}
	
	//Change MODE for user/channel
	User *tmpUser;

	std::string object = input.getParams()[0];
	std::string flags = input.getParams()[1];

	if (!checkModeFlags(object, flags)) //(x < 0 ? x : (x >= 0)
	{
		if (flags.size() == 1 && (flags[0] == '+' || flags[0] == '-'))
			return (-1);
		if (object[0] == '#' || object[0] == '&')
			return sendServerReply(user, ERR_UNKNOWNMODE, flags);
		else
			return sendServerReply(user, ERR_UMODEUNKNOWNFLAG);
	}

	if (object[0] == '#' || object[0] == '&') // channel
	{
		Channel *channel;
		try { channel = _channels.at(object); }
		catch (const std::exception &e) { return sendServerReply(user, ERR_NOSUCHCHANNEL, object); }

		if (flags[0] == '+' && flags[1] == 'b' && input.getParams().size() == 2)
		{
			channel->sendBans(user);
			return (0);
		}

		if (!channel->isOperator(user))
			return sendServerReply(user, ERR_CHANOPRIVSNEEDED, channel->getName());

		std::string argument;
		for (int i = 1; flags[i] != '\0'; i++)
		{
			unsigned char flag = 0;
			switch (flags[i])
			{
				case 'p':
					flag = CHL_PRIVATE;
					break;
				case 's':
					flag = CHL_SECRET;
					break;
				case 'i':
					flag = CHL_INVITEONLY;
					break;
				case 't':
					flag = CHL_TOPICSET;
					break;
				case 'n':
					flag = CHL_NOMSGOUT;
					break;
				case 'm':
					flag = CHL_MODERATED;
					break;
			}
			if (flag != 0 && flags[0] == '-')
				channel->clearFlag(flag);
			else if (flag != 0)
				channel->setFlag(flag);
			if (flag != 0)
				continue ;

			if (input.getParams().size() < 3)
				return sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());

			argument = input.getParams()[2];
			if (flags[i] == 'o')
			{
				tmpUser = this->searchUser(SRCH_NICK, argument);
				if (!tmpUser)
					return sendServerReply(user, ERR_NOSUCHNICK, argument);
				if (flags[0] == '-')
					channel->deleteOperator(*tmpUser);
				else
					channel->addOperator(*tmpUser);
			}
			if (flags[i] == 'l')
			{
				if (flags[0] == '-')
					channel->removeLimit();
				else
				{
					try { channel->setLimit(std::stoi(argument)); }
					catch(...) {}
				}
			}
			if (flags[i] == 'b')
			{
				tmpUser = this->searchUser(SRCH_NICK, argument);
				if (!tmpUser)
					return sendServerReply(user, ERR_NOSUCHNICK, argument);
				if (flags[0] == '-')
					channel->removeFromBan(*tmpUser);
				else
					channel->addInBan(*tmpUser);
			}
			if (flags[i] == 'v')
			{
				tmpUser = this->searchUser(SRCH_NICK, argument);
				if (!tmpUser)
					return sendServerReply(user, ERR_NOSUCHNICK, argument);
				if (flags[0] == '-')
					channel->removeInSpeakers(*tmpUser);
				else
					channel->addInSpeakers(*tmpUser);
			}
			if (flags[i] == 'k')
			{
				if (flags[0] == '+')
					channel->setPass(argument);
				else
					channel->removePass();
			}
		}
		std::string msg = "MODE " + object + " " + flags;
		if (argument.size())
			msg += " " + argument;
		user.sendMessage(":" + user.getMask() + " " + msg);
		channel->sendNotification(msg, user);
		return (0);
	}
	else
	{
		if (user.getNick() != object)
			return sendServerReply(user, ERR_USERSDONTMATCH);
		tmpUser = this->searchUser(SRCH_NICK, object);
		if (!tmpUser)
			return sendServerReply(user, ERR_USERSDONTMATCH, object);
		for (int i = 1; flags[i] != '\0'; i++)
		{
			unsigned char flag;
			switch (flags[i])
			{
			case 'i':
				flag = USER_INVISIBLE;
				break;
			case 's':
				flag = USER_GETNOTICE;
				break;
			case 'o':
				flag = USER_OPERATOR;
				break;
			case 'w':
				flag = USER_GETWALLOPS;
				break;
			}
			if (flags[0] == '-')
				tmpUser->clearFlags(flag);
			else if (flag != USER_OPERATOR)
				tmpUser->setFlags(flag);
		}
		std::string msg = "MODE " + object + " " + flags;
		user.sendMessage(":" + user.getMask() + " " + msg);
		return (0);
	}
	return (0);
}


static int pm_or_notice(User &user, Input &input, int code, int silent)
{
	if (silent)
		return (-1);
	return (sendServerReply(user, code, input.getParams()[0]));
}

static int pm_or_notice(User &user, Input &input, int code, int silent, const std::string &name)
{
	(void) input;
	if (silent)
		return (-1);
	return (sendServerReply(user, code, name));
}

int Server::sendPM(User &user, Input &input, int silent)
{
	if (input.getParams().size() == 0) //Only command present
		return (pm_or_notice(user, input, ERR_NORECIPIENT, silent));
	if (input.getParams().size() == 1) //Only command and recipient present
		return (pm_or_notice(user, input, ERR_NEEDMOREPARAMS, silent));
	if (input.getParams()[1].size() == 0) //Empty message (just "")
		return (pm_or_notice(user, input, ERR_NOTEXTTOSEND, silent));

	std::queue<std::string> receivers = User::split(input.getParams()[0], ',', 1);
	std::set<std::string> finalReceivers;
	std::pair<std::set<std::string>::iterator, bool> check_pair;

	while (receivers.size() != 0)
	{
		int check; //переменная, показывающая что сервер отправил ошибку
		check_pair = finalReceivers.insert(receivers.front());
		if (check_pair.second == false)
		{
			receivers.pop();
			continue ;
		}
		if (receivers.front()[0] == '#' || receivers.front()[0] == '&') //Для каналов
		{
			check = 0;
			std::string channel = receivers.front();
			if (!containsChannel(channel)) //Нет канала
				check = pm_or_notice(user, input, ERR_NOSUCHNICK, silent, channel);
			else if (!_channels[channel]->isChannelUser(user.getNick())) //Нет юзера на канале
			{
				if (_channels[channel]->getFlags() & CHL_NOMSGOUT) //Не разрешены внешние сообщения
					check = pm_or_notice(user, input, ERR_CANNOTSENDTOCHAN, silent);
				else if (_channels[channel]->getFlags() & CHL_MODERATED) //Нельзя писать на модерируемый
					check = pm_or_notice(user, input, ERR_CANNOTSENDTOCHAN, silent);
			}
			//Юзер на канале, остается узнать есть ли права писать, если канал модерируется
			else if (_channels[channel]->getFlags() & CHL_MODERATED
					&& !_channels[channel]->isOperator(user)
					&& !_channels[channel]->isSpeaker(user))
				check = pm_or_notice(user, input, ERR_CANNOTSENDTOCHAN, silent);

			//Если в верхние if не зашли (т.е. все правильно), то отправляем сообщение
			if (!check)
			{
				std::string msg = input.getCommand() + " " + channel + " :" + input.getParams()[1];
				_channels[channel]->sendNotification(msg, user);
			}
		}
		else //Для юзеров
		{
			check = 0;
			std::string trueNick = getTrueNickname(receivers.front());
			if (!containsNickname(trueNick))
				check = pm_or_notice(user, input, ERR_NOSUCHNICK, silent, receivers.front());
			if (!check)
			{
				User *recipient = searchUser(SRCH_NICK, trueNick);
				std::string msg = ":" + user.getMask() + " " + input.getCommand() + " " + recipient->getNick() + " :" + input.getParams()[1];
				recipient->sendMessage(msg);
			}
		}
		receivers.pop();
	}

	return (0);
}

std::string Server::getTrueNickname(std::string &mask)
{
	std::string nickName;
	size_t found = mask.find("!");
	if (found != std::string::npos)
		nickName = std::string(mask.begin(), mask.begin() + found);
	else
		nickName = mask;
	return (nickName);
}

int Server::privmsg(User &user, Input &input)
{
	return sendPM(user, input, 0);
}

int Server::notice(User &user, Input &input)
{
	return sendPM(user, input, 1);
}

int Server::topic(User &user, Input &input) {

	if (input.getParams().size() < 1)//Мало аргументов
		sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());
	else if (!containsChannel(input.getParams()[0]))//Нет такого канала
		sendServerReply(user, ERR_NOTONCHANNEL, input.getParams()[0]);
	else {
		Channel *c = _channels.at(input.getParams()[0]);
		if (!c->isChannelUser(user.getNick()))
			sendServerReply(user, ERR_NOTONCHANNEL, input.getParams()[0]);
		else if (input.getParams().size() < 2)
			c->sendTopic(user);
		else
			c->setTopic(user, input.getParams()[1]);
	}
	return 0;
}

int Server::names(User &user, Input &input)
{
	if (input.getParams().size() == 0)
	{
		std::set<std::string> unboundUsers; // Все юзеры вначале, юзеры без канала в конце
		std::set<std::string> channelUsers; // Юзеры в видимом канале
		for (size_t i = 0; i < _users.size(); i++)
		{
			if (!(_users[i]->getFlags() & USER_INVISIBLE))
				unboundUsers.insert(_users[i]->getNick());
		}

		std::map<std::string, Channel *>::iterator it = _channels.begin();
		std::map<std::string, Channel *>::iterator end = _channels.end();
		//Displaying channel names and users within
		for (; it != end; it++)
		{
			//Check that channel is not private or secret. If it is, then check if user is a member
			if (((*it).second->getFlags() & CHL_PRIVATE || (*it).second->getFlags() & CHL_SECRET)
				&& !(*it).second->isChannelUser(user.getNick()))
				continue ;
			(*it).second->sendChannelUsers(user);
			//Filling in channel users for future reduction
			std::vector<const User *> usersOnChannel = (*it).second->getUsers();
			for (size_t i = 0; i < usersOnChannel.size(); i++)
			{
				if (!(usersOnChannel[i]->getFlags() & USER_INVISIBLE))
					channelUsers.insert(usersOnChannel[i]->getNick());
			}
		}
		//Removing channel users from all users
		while (channelUsers.size())
		{
			unboundUsers.erase(*channelUsers.begin());
			channelUsers.erase(*channelUsers.begin());
		}
		//Sending channelless users info to user
		while (unboundUsers.size())
		{
			std::string nicks = *unboundUsers.begin();
			unboundUsers.erase(*unboundUsers.begin());
			while (nicks.size() < 450 && unboundUsers.size())
			{
				nicks += " ";
				nicks += *unboundUsers.begin();
				unboundUsers.erase(*unboundUsers.begin());
			}
			sendServerReply(user, RPL_NAMREPLY, "* *", nicks);
		}
		sendServerReply(user, RPL_ENDOFNAMES, "*");
	}
	else
	{
		std::queue<std::string> channelNames;
		channelNames = User::split(input.getParams()[0], ',', 1);
		while (channelNames.size())
		{
			try
			{
				Channel * temp = _channels.at(channelNames.front());
				if ((temp->getFlags() & CHL_PRIVATE || temp->getFlags() & CHL_SECRET)
					&& !temp->isChannelUser(user.getNick()))
				{
					channelNames.pop();
					continue ;
				}
				temp->sendChannelUsers(user);
			}
			catch (std::exception &e) {}
			sendServerReply(user, RPL_ENDOFNAMES, channelNames.front());
			channelNames.pop();
		}
	}
	return (0);
}

int Server::list(User &user, Input &input)
{
	//No server found
	if (input.getParams().size() >= 2 && input.getParams()[1] != ircName)
		return (sendServerReply(user, ERR_NOSUCHSERVER, input.getParams()[1]));

	std::vector<Channel *> channelList; //channels to display info about

	//LIST with parameters
	if (input.getParams().size() >= 1)
	{
		std::queue<std::string> channelNames = User::split(input.getParams()[0], ',', 1);
		while (channelNames.size())
		{
			if (containsChannel(channelNames.front()))
				channelList.push_back(_channels.at(channelNames.front()));
			channelNames.pop();
		}
	}
	//No parameters
	else
	{
		std::map<std::string, Channel *>::iterator it = _channels.begin();
		std::map<std::string, Channel *>::iterator end = _channels.end();
		for (; it != end; it++)
			channelList.push_back((*it).second);
	}
	sendServerReply(user, RPL_LISTSTART);
	for (size_t i = 0; i < channelList.size(); i++)
		channelList[i]->sendChannelInfo(user);
	sendServerReply(user, RPL_LISTEND);
	return (0);
}

int Server::quit(User &user, Input &input)
{
	if (input.getParams().size())
		user.setQuitMessage("Quit: " + input.getParams()[0]);
	return (SERVER_DISCONNECT);
}

int Server::oper(User &user, Input &input)
{
	if (input.getParams().size() != 2)
		return sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand());
	try
	{
		if (_operators.at(input.getParams()[0]) == input.getParams()[1])
		{
			user.setFlags(USER_OPERATOR);
			return sendServerReply(user, RPL_YOUREOPER);
		}
	}
	catch(const std::exception& e) {}
	return sendServerReply(user, ERR_PASSWDMISMATCH);
}

int Server::kill(User &user, Input &input)
{
	if (!(user.getFlags() & USER_OPERATOR))
		return (sendServerReply(user, ERR_NOPRIVILEGES));
	else if (input.getParams().size() < 2)
		return (sendServerReply(user, ERR_NEEDMOREPARAMS, input.getCommand()));
	else if (input.getParams()[0] == ircName)
		return (sendServerReply(user, ERR_CANTKILLSERVER));
	else if (!containsNickname(input.getParams()[0]))
		return (sendServerReply(user, ERR_NOSUCHNICK, input.getParams()[0]));

	User *userToKill = searchUser(SRCH_NICK, input.getParams()[0]);
	std::string msg = ":" + user.getMask() + " KILL " + userToKill->getNick() + " :" + input.getParams()[1];
	userToKill->sendToAllUserChannels(msg);
	userToKill->setQuitMessage("KILL :" + input.getParams()[1]);
	disconnectUser(*userToKill, SERVER_KILL);
	return (0);
}

int Server::invite(User &user, Input &input) {

    if (input.getParams().size() < 2)
        sendServerReply(user, ERR_NEEDMOREPARAMS, "INVITE");
    else if (!containsNickname(input.getParams()[0]))
        sendServerReply(user, ERR_NOSUCHNICK, input.getParams()[0]);
    else if (!user.isChannelMember(input.getParams()[1]) || !containsChannel(input.getParams()[1]))
        sendServerReply(user, ERR_NOTONCHANNEL, input.getParams()[1]);
    else {

        User	*userToInvite;
        for (size_t i = 0; i < _users.size(); ++i)
            if (_users[i]->getNick() == input.getParams()[0])
                userToInvite = _users[i];
        Channel	*channelToInvite = _channels.at(input.getParams()[1]);
        if (channelToInvite->isChannelUser(input.getParams()[1]))
            sendServerReply(user, ERR_USERONCHANNEL, input.getParams()[0], input.getParams()[1]);
        else
            channelToInvite->inviteToChannel(user, *userToInvite);
    }

    return 0;
}

int Server::ping(User &user, Input &input)
{
	if (input.getParams().size() == 0)
		return (sendServerReply(user, ERR_NOORIGIN));
	if (input.getParams().size() == 1)
		user.sendMessage(":" + std::string(ircName) + " PONG :" + input.getParams()[0]);
	else
		user.sendMessage(":" + std::string(ircName) + " PONG :" + input.getParams()[1]);
	return 0;
}
