#ifndef DECODER_H
#define DECODER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

struct DecodeData
{
	int ndx;
	int ndy;
	QVector<QMap<QString, int>> piece;
	bool vaild;
};

class Decoder
{
	public:
		Decoder(QString bookID, QString decodeKey, QString ptbl, QString ctbl, QString stbl, QString ttbl);
		bool DecodeImage(QString inName, QByteArray imgData, QString outName);
		void DecodeB64Image(QByteArray imgData, QString outName);
		QString DecodeCharacter(QString str);

	private:
		void calcDecodeCode(QString bookID, QString userKey);
		QJsonArray decodeData(QString data);

		QList<QList<int> > calcTNP(QString str, int h, int v);

		bool mkPath(QString name);

	private:
		unsigned long m_decryptCode;
		QJsonArray m_ptbl;
		QJsonArray m_ctbl;
		QJsonArray m_stbl;
		QJsonArray m_ttbl;

		static int m_arr[128];
};

#endif // DECODER_H
