#include "networkinterface.h"
#include "widget.h"





//La méthode statique d'allocation
NetworkInterface *NetworkInterface::createNetworkInterface(ConfigurationNetwork *configurationNetwork,ConfigurationIdentification *configurationIdentification, QStandardItemModel *model)
{
	//On teste la validité de la configuration
	if(configurationNetwork==NULL || configurationIdentification==NULL)
	{
		if(model!=NULL) Widget::addRowToTable("Echec lors de l'allocation du module d'interface réseau.",model,MSG_1);
		return NULL;
	}

	//On retourne l'objet créé
	return new NetworkInterface(configurationNetwork,configurationIdentification,model);
}




//Le constructeur
NetworkInterface::NetworkInterface(ConfigurationNetwork *configurationNetwork, ConfigurationIdentification *configurationIdentification, QStandardItemModel *model): QThread()
{
	this->moveToThread(this);

	//On crèe la socket
	this->socket=new Socket();
	this->configurationNetwork=configurationNetwork;
	this->configurationIdentification=configurationIdentification;

	//On fait les initialisations
	this->isConnected=false;
	this->isIdentified=false;
	this->waitReceiveRequestList=NULL;
	this->receiveRequestList=new QList<Request*>();
	blockDisconnectedMutex.lock();

	//On établit les connexion des évenement de socket à la classe.
	QObject::connect(socket,SIGNAL(stateChanged(QAbstractSocket::SocketState)),this,SLOT(stateChangedAction(QAbstractSocket::SocketState)),Qt::DirectConnection);
	QObject::connect(socket,SIGNAL(connected()),this,SLOT(connectedToServer()));
	QObject::connect(socket,SIGNAL(disconnected()),this,SLOT(disconnectedFromServer()));

	QObject::connect(socket,SIGNAL(encrypted()),this,SLOT(connexionEncrypted()),Qt::DirectConnection);
	QObject::connect(socket,SIGNAL(sslErrors(QList<QSslError>)),this,SLOT(erreursSsl(QList<QSslError>)),Qt::DirectConnection);
	QObject::connect(socket,SIGNAL(receiveMessage(QByteArray*)),this,SLOT(receiveMessageAction(QByteArray*)),Qt::DirectConnection);

	this->model = model;
	Widget::addRowToTable("Le module d'interface réseau a bien été alloué.",model,MSG_1);
}





//Slot appelé lorsque l'état de la socket a changé
void NetworkInterface::stateChangedAction(QAbstractSocket::SocketState state)
{
	//Le nouvel état est dans state. Selon sa valeur, on rédige une description à afficher
	QString description;
	if(state==QAbstractSocket::UnconnectedState) description="L'application déconnectée du serveur";
	else if(state==QAbstractSocket::HostLookupState) description="L'application recherche le serveur à l'adresse: "+configurationNetwork->getFullAddress();
	else if(state==QAbstractSocket::ConnectingState) description="L'application tente de se connecter au serveur";
	else if(state==QAbstractSocket::ConnectedState) description="L'application est connectée au serveur en mode non crypté";
	else if(state==QAbstractSocket::ClosingState) description="L'application coupe sa connexion au serveur";
	else description="La connexion réseau est à un état inconnu";

	Widget::addRowToTable("socket::stateChanged: "+description,model,MSG_2);
}



//Slot appelé lorsque la socket est connectée
void NetworkInterface::connectedToServer()
{
	Widget::addRowToTable("Connexion établie",model,MSG_2);
}



//Slot appelé lorsque la socket est déconnectée
void NetworkInterface::disconnectedFromServer()
{
	this->isConnected=false;
	this->isIdentified=false;
	blockDisconnectedMutex.lock();
	Widget::addRowToTable("Connexion perdue",model,MSG_3);
	emit disconnected();
}



//Recu quand la connexion a été correctement cryptée
void NetworkInterface::connexionEncrypted()
{
	this->isConnected=true;
	blockDisconnectedMutex.unlock();
	Widget::addRowToTable("Connexion avec le serveur correctement établie et cryptée",model,MSG_2);
	emit connected();
}




//Erreurs SSL recues pendant la phase de handshake
void NetworkInterface::erreursSsl(const QList<QSslError> &errors)
{
	foreach(const QSslError &error, errors)
	{
		Widget::addRowToTable("Erreur SSL ignorée: "+ error.errorString(), model,MSG_3);
	}
}



//La méthode exec pour lancer le thread
void NetworkInterface::run()
{
	exec();
}


//Savoir si on est connecté au serveur
bool NetworkInterface::checkIsConnected()
{
	return isConnected;
}


//Bloquer le thread appelant tant qu'on est pas connecté
void NetworkInterface::blockWhileDisconnected()
{
	blockDisconnectedMutex.lock();
	blockDisconnectedMutex.unlock();
}


//Pour se connecter au serveur
//Cette fonction est bloquante pendant quelques secondes
//elle doit être appelée par un thread externe (si gui revoir socket.connect)
bool NetworkInterface::connectToServer()
{
	Widget::addRowToTable("Tentative de connexion au serveur",model,MSG_1);
	bool a=socket->connectToServer(configurationNetwork->getAddress(),configurationNetwork->getPort());
	if(a) Widget::addRowToTable("Success: Connexion réuissie",model,MSG_2);
	else Widget::addRowToTable("Echec: Connexion échouée",model,MSG_3);
	return a;
}



//Pour se déconnecter du servuer
//Cette fonction est bloquante pendant quelques secondes
//elle doit être appelée par un thread externe (si gui revoir socket.disconnect)
bool NetworkInterface::disconnectFromServer()
{
	return socket->disconnectFromServer();
}




//Ce slot est apellé lorsqu'un message est recu par la socket
void NetworkInterface::receiveMessageAction(QByteArray *message)
{
	//On récupère le message
	Message *m=Messages::parseMessage(message);
	//Si le message est inconnu on emet un signal d'erreur
	if(!m){emit receiveErrorMessage("NON_XML_MESSAGE");return;}

	//Si c'est une requete
	if(m->isRequest())
	{
		Request *r=(Request*)m;
		putReceiveRequestList(r);
		return;
	}
	//sinon une réponse
	else
	{
		Response *r=(Response*)m;
		response=r->getType();
		waitMessages.wakeAll();
		return;
	}

	//Si on ne trouve pas le type de message, on emet un signal d'érreur.
	emit receiveErrorMessage("UNKNOWN_MESSAGE");
	return;
}




//Pour envoyer un message d'identification
//cette fonction est bloquante tant qu'elle ne recoit pas de réponse
//il l'appeller dans un thread externe
ResponseEnum NetworkInterface::sendIdentification()
{
	//On vérifie que la socket est bien connectée et qu'on est identifié
	if(!isConnected) return NOT_CONNECT;
	isIdentified=true;

	//On récupère le pseudo et le mot de passe de la configuration d'identification
	QString pseudo=configurationIdentification->getPseudo();
	QString password=configurationIdentification->getPassword();

	Request request;
	request.setType(IDENTIFICATION);
	request.getParameters()->insert("pseudo",pseudo.toAscii());
	request.getParameters()->insert("password",password.toAscii());

	//On crèe le message d'identification
	QByteArray *message=request.toXml();
	bool r=socket->sendMessage(message);
	if(!r) return NOT_SEND;

	QMutex mutex;
	if(!waitMessages.wait(&mutex,20000)) return NOT_TIMEOUT;

	return response;
}



//Pour envoyer un message de fichier modifié
ResponseEnum NetworkInterface::sendMediaUpdated(QString realPath,QByteArray content)
{
	//On vérifie que la socket est bien connectée et qu'on est identifié
	if(!isConnected) return NOT_CONNECT;
	if(!isIdentified) return NOT_IDENTIFICATE;

	//On vérifie que le realPath n'est pas vide
	if(realPath.isEmpty()) return NOT_PARAMETERS;

	Request request;
	request.setType(UPDATE_FILE_INFO);
	request.getParameters()->insert("realPath",realPath.toAscii());
	request.getParameters()->insert("content",content);

	//On rédige le message et on l'envoi
	QByteArray *message=request.toXml();
	bool r=socket->sendMessage(message);
	if(!r) return NOT_SEND;

	QMutex mutex;
	if(!waitMessages.wait(&mutex,20000)) return NOT_TIMEOUT;

	return response;
}



//Pour envoyer un message de média créé
ResponseEnum NetworkInterface::sendMediaCreated(QString realPath, bool isDirectory)
{
	//On vérifie que la socket est bien connectée et qu'on est identifié
	if(!isConnected) return NOT_CONNECT;
	if(!isIdentified) return NOT_IDENTIFICATE;

	//On vérifie que le realPath n'est pas vide
	if(realPath.isEmpty()) return NOT_PARAMETERS;

	Request request;
	request.setType(CREATE_FILE_INFO);
	request.getParameters()->insert("realPath",realPath.toAscii());
	request.getParameters()->insert("isDirectory",isDirectory?"true":"false");

	//On rédige le message et on l'envoi
	QByteArray *message=request.toXml();
	bool r=socket->sendMessage(message);
	if(!r) return NOT_SEND;

	QMutex mutex;
	if(!waitMessages.wait(&mutex,20000)) return NOT_TIMEOUT;

	return response;
}






//Envoyer un message de média supprimé
ResponseEnum NetworkInterface::sendMediaRemoved(QString realPath)
{
	//On vérifie que la socket est bien connectée et qu'on est identifié
	if(!isConnected) return NOT_CONNECT;
	if(!isIdentified) return NOT_IDENTIFICATE;

	//On vérifie que le realPath n'est pas vide
	if(realPath.isEmpty()) return NOT_PARAMETERS;

	Request request;
	request.setType(REMOVE_FILE_INFO);
	request.getParameters()->insert("realPath",realPath.toAscii());

	//On rédige le message et on l'envoi
	QByteArray *message=request.toXml();
	bool r=socket->sendMessage(message);
	if(!r) return NOT_SEND;

	QMutex mutex;
	if(!waitMessages.wait(&mutex,20000)) return NOT_TIMEOUT;

	return response;
}





//Pour récupérer la prochaine requete à traiter
Request *NetworkInterface::getReceiveRequestList()
{
	receiveRequestListMutex.lock();
	Request *r;
	if(receiveRequestList->size()>0)
	{
		r=receiveRequestList->first();
		receiveRequestList->removeFirst();
	}
	else r=NULL;
	receiveRequestListMutex.unlock();
	return r;
}




//pour ajouter une requete à traiter prochainement
void NetworkInterface::putReceiveRequestList(Request *r)
{
	receiveRequestListMutex.lock();
	receiveRequestList->append(r);
	if(waitReceiveRequestList!=NULL) waitReceiveRequestList->wakeAll();
	receiveRequestListMutex.unlock();
}



//Indiquer l'objet de reveil du controleur
void NetworkInterface::setWaitReceiveRequestList(QWaitCondition *waitReceiveRequestList)
{
	this->waitReceiveRequestList=waitReceiveRequestList;
}



