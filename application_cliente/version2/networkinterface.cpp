#include "networkinterface.h"



//La méthode statique d'allocation
NetworkInterface *NetworkInterface::createNetworkInterface(ConfigurationData *configurationData)
{
	//On teste la validité de la configuration
	if(configurationData==NULL) return NULL;

	//On retourne l'objet créé
	return new NetworkInterface(configurationData);
}


//Le constructeur
NetworkInterface::NetworkInterface(ConfigurationData *configurationData): QObject()
{
	//On crèe la socket
	socket=new Socket();

	//On établit les connexion des évenement de socket à la classe.
	QObject::connect(socket,SIGNAL(stateChanged(QAbstractSocket::SocketState)),this,SLOT(stateChangedAction(QAbstractSocket::SocketState)));
	QObject::connect(socket,SIGNAL(connected()),this,SIGNAL(connectedToServer()));
	QObject::connect(socket,SIGNAL(disconnected()),this,SIGNAL(disconnectedFromServer()));
	QObject::connect(socket,SIGNAL(receiveMessage(QByteArray*)),this,SLOT(receiveMessageAction(QByteArray*)));

	//On initialise la socket
	this->configurationData=configurationData;
}


//Pour se connecter au serveur
bool NetworkInterface::connect()
{
	//C'est le socket qui s'en charge
	return socket->connectToServer(configurationData->getConfigurationNetwork()->getAddress(),configurationData->getConfigurationNetwork()->getPort());
}


//Pour se déconnecter du servuer
bool NetworkInterface::disconnect()
{
	//C'est le socket qui s'en charge
	return socket->disconnectFromServer();
}


//Ce slot est apellé lorsqu'un message est recu par la socket
void NetworkInterface::receiveMessageAction(QByteArray *message)
{

	//On parse pour récupérer le message
	QHash<QString,QByteArray> *arg=Messages::parseMessage(message);
        if(!arg){ emit receiveErrorMessage("4"); return; }


	//On récupere le realPath
	QString realPath=arg->value("realPath","");
        if(realPath.isEmpty()){ emit receiveErrorMessage("5"); return; }

        //En fonction de l'action demandée, on emet le signal correspondant
	if(arg->value("action")=="MODIFIED")
	{

		//On récupère le média à modifier
		Media *m=configurationData->getConfigurationFile()->findMediaByRealPath(realPath);
                if(m==NULL){ emit receiveErrorMessage("2"); return; }
                if(m->isDirectory()){ emit receiveErrorMessage("3"); return; }

		File *f=(File*)m;

		//On récupère le contenu du fichier
		QByteArray content=arg->value("content","");

		//On émet le signal de modification de fichier
		emit receiveModifiedFileMessage(f,content);
		return;
	}

	//Sinon, si c'est une action de création
	else if(arg->value("action")=="CREATED")
	{

		QString realPath2=realPath;
		if(realPath2.endsWith("/")) realPath2=realPath2.left(realPath2.length()-1);

		//On récupère le média parent
		Media *parentMedia=configurationData->getConfigurationFile()->findMediaByRealPath(realPath2.left(realPath2.lastIndexOf("/")));

		//On vérifie que le parent n'est pas NULL et que c'est bien un repertoire
		if(parentMedia==NULL){emit receiveErrorMessage("7");return;}
		if(!parentMedia->isDirectory()){emit receiveErrorMessage("10");return;}

		//On cast le parent en Dir
		Dir *parent=(Dir*)parentMedia;

		//On récupère le nom du média créé. NB: Si c'est un repertoire on fait terminé le nom par "/"
		QString realName=realPath2.right(realPath2.length()-realPath2.lastIndexOf("/")-1);
		if(realPath.endsWith("/")) realName=realName+"/";

		//On émet le signal de création du média
		emit receiveCreatedMediaMessage(parent,realName);
		return;
	}

	//Sinon si c'est une action de suppression
	else if(arg->value("action")=="REMOVED")
	{

		//On récupère le média supprimé
		Media *m=configurationData->getConfigurationFile()->findMediaByRealPath(realPath);

		//On vérifie qu'il n'est pas NULL
		if(m==NULL){emit receiveErrorMessage("6");return;}

		//On émet un signal de suppression du média
		emit receiveRemovedMediaMessage(m);
		return;
	}
	else if(arg->value("action")=="VALIDATION")
	{

		//On émet un signal de validation
		emit receiveValidationMessage();
		return;
	}
	else if(arg->value("action")=="ANNULATION")
	{

		//on émet un signal d'annulation
		emit receiveAnnulationMessage();
		return;
	}

	//Si on ne trouve pas le type de message, on emet un signal d'érreur.
	emit receiveErrorMessage("8");
	return;
}




//Pour envoyer un message d'identification
bool NetworkInterface::sendIdentification()
{
	//On vérifie que la socket est bien connectée
	if(!socket->isWritable()) return false;

	//On récupère le pseudo et le mot de passe de la configuration d'identification
	QString pseudo=configurationData->getConfigurationIdentification()->getPseudo();
	QString password=configurationData->getConfigurationIdentification()->getPassword();

	//On crèe le message d'identification
	QByteArray *message=Messages::createIdentificationMessage(pseudo,password);

	//On envoi le message avec la socket et on retourne le resultat
	return socket->sendMessage(message);
}



//Pour envoyer un message de fichier modifié
bool NetworkInterface::sendFileModified(QString realPath,QByteArray content)
{
	//On vérifie que la socket est bien connectée
	if(!socket->isWritable()) return false;

	//On vérifie que le realPath n'est pas vide
	if(realPath.isEmpty()) return false;

	//On rédige le message et on l'envoi
	QByteArray *message=Messages::createFileContentMessage(realPath,content);
	return socket->sendMessage(message);
}


//Pour envoyer un message de média créé
bool NetworkInterface::sendMediaCreated(QString realPath)
{
	//On vérifie que la socket est connectée
        if(!socket->isWritable())
            return false;
        if(realPath.isEmpty())
            return false;

	  //On rédige le message et on l'envoi
	QByteArray *message=Messages::createMediaCreatedMessage(realPath);
	return socket->sendMessage(message);
}



//Envoyer un message de média supprimé
bool NetworkInterface::sendMediaRemoved(QString realPath)
{
	//On vérifie que la socket est bien connectée
	if(!socket->isWritable()) return false;
	if(realPath.isEmpty()) return false;

	//On rédihe le message et on l'envoi
	QByteArray *message=Messages::createMediaRemovedMessage(realPath);
	return socket->sendMessage(message);
}


//Slot appelé lorsque l'état de la socket a changé
void NetworkInterface::stateChangedAction(QAbstractSocket::SocketState state)
{
	//Le nouvel état est dans state.
	//Selon sa valeur, on rédige une description à renvoyer
	QString description;
	if(state==QAbstractSocket::UnconnectedState) description="L'application est en mode déconnecté du serveur";
	else if(state==QAbstractSocket::HostLookupState) description="L'application recherche le serveur à l'adresse: "+configurationData->getConfigurationNetwork()->getAddress();
	else if(state==QAbstractSocket::ConnectingState) description="L'application tente de se connecter au serveur";
	else if(state==QAbstractSocket::ConnectedState) description="L'application est en mode connecté au serveur";
	else if(state==QAbstractSocket::ClosingState) description="L'application est en cours de fermeture de la connexion";
	else description="La connexion réseau est à un état inconnu";

	//On émet un signal contenant l'état et sa description
	emit connexionStateChanged(state,description);
}

