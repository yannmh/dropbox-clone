#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include <QStandardItemModel>
#include "socket.h"


/*
 Un objet de cette classe est alloué à chaque client qui se connecte.
 Il est chargé de communiquer avec le client.
*/

class ClientManager: public QObject
{
	Q_OBJECT

public:
	//Une méthode statique pour allocation
	static ClientManager *createClientManager(int clientSocket,QVector<ClientManager*> *clients,QStandardItemModel *model);

signals:
	//Emis pour dire qu'un client est parti, et donc qu'on arrete le client manager
	void clientManagerStop(ClientManager*);

public slots:
	 //Le slot levé lorsqu'un message est recu
	 void receiveMessageAction(QByteArray *message);

        //Lorsqu'un client est déconnecté
        void clientDisconnected();

        //On recoit des erreurs lors de la connexion SSL
        void erreursSsl(const QList<QSslError>&);

        void connexionEncrypted();

private:
	ClientManager(QVector<ClientManager*> *clients,QStandardItemModel *model);

	//La socket communiquant avec le client
	Socket *socket;

	//La liste des autres clients
	QVector<ClientManager*> *clients;
	QStandardItemModel *model;
};

#endif // CLIENTMANAGER_H
