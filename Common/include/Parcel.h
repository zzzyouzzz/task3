#pragma once
#include <string>
#include <sstream>
#include <ctime>
#include "common.h"

enum class ParcelType {
    NORMAL = 0,
    FRAGILE,
    BOOK
};

enum class ParcelStatus {
    PENDING_COLLECTION = 0,
    PENDING_SIGN,
    SIGNED,
    OTHER
};

// ================== 快递类体系 ==================
class Parcel {
protected:
    std::string m_parcelId;       // 快递单号
    std::string m_senderName;     // 寄件人用户名
    std::string m_receiverName;   // 收件人用户名
    time_t m_sendTime;            // 寄送时间
    time_t m_receiveTime;         // 签收时间
    ParcelStatus m_status;        // 快递状态
    std::string m_description;    // 物品描述
    double m_weight;              // 重量/数量
    std::string m_courierName;    // 分配的快递员用户名
    ParcelType m_type;            // 快递类型

public:
    Parcel() = default;
    Parcel(ParcelType type, const std::string& id, const std::string& sender, const std::string& receiver,
           time_t sendTime, time_t receiveTime, ParcelStatus status, const std::string& desc, double weight,    
           const std::string& courierName)
        : m_parcelId(id), m_senderName(sender), m_receiverName(receiver),
          m_sendTime(sendTime), m_receiveTime(receiveTime),
          m_status(status), m_description(desc),
          m_weight(weight), m_courierName(courierName), m_type(type) {}
    Parcel(const std::string& id, const std::string& sender, const std::string& receiver,
           double weight, const std::string& desc, ParcelType type)
        : m_parcelId(id), m_senderName(sender), m_receiverName(receiver),
          m_sendTime(time(nullptr)), m_receiveTime(0),
          m_status(ParcelStatus::PENDING_COLLECTION), m_description(desc),
          m_weight(weight), m_courierName(""), m_type(type) {}

    virtual ~Parcel() = default;

    // 计算快递费用，由子类覆盖
    virtual double getPrice() const { return 0.0; };

    std::string getParcelId() const { return m_parcelId; }
    std::string getSenderName() const { return m_senderName; }
    std::string getReceiverName() const { return m_receiverName; }
    ParcelStatus getStatus() const { return m_status; }
    std::string getDescription() const { return m_description; }
    double getWeight() const { return m_weight; }
    std::string getCourierName() const { return m_courierName; }
    ParcelType getParcelType() const { return m_type; }
    time_t getSendTime() const { return m_sendTime; }
    time_t getReceiveTime() const { return m_receiveTime; }

    void setCourier(const std::string& name) { m_courierName = name; }
    void setStatus(ParcelStatus s) { m_status = s; }
    void setReceiveTime(time_t t) { m_receiveTime = t; }

    virtual std::string serialize() const {
        std::ostringstream oss;
        oss << static_cast<int>(m_type) << DELIMITER
            << m_parcelId << DELIMITER << m_senderName << DELIMITER
            << m_receiverName << DELIMITER << m_sendTime << DELIMITER
            << m_receiveTime << DELIMITER << static_cast<int>(m_status) << DELIMITER
            << m_description << DELIMITER << m_weight << DELIMITER
            << m_courierName;
        return oss.str();
    }

};

class NormalParcel : public Parcel {
public:
    NormalParcel() = default;
    NormalParcel(const std::string& id, const std::string& sender, const std::string& receiver,
                 double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::NORMAL) {}
    NormalParcel(ParcelType type, const std::string& id, const std::string& sender, const std::string& receiver,
                 time_t sendTime, time_t receiveTime, ParcelStatus status, const std::string& desc, double weight,    
                 const std::string& courierName)
        : Parcel(type, id, sender, receiver, sendTime, receiveTime, status, desc, weight, courierName) {}
    double getPrice() const override { return 5.0 * getWeight(); }
};

class FragileParcel : public Parcel {
public:
    FragileParcel() = default;
    FragileParcel(const std::string& id, const std::string& sender, const std::string& receiver,
                  double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::FRAGILE) {}
    FragileParcel(ParcelType type, const std::string& id, const std::string& sender, const std::string& receiver,
                 time_t sendTime, time_t receiveTime, ParcelStatus status, const std::string& desc, double weight,    
                 const std::string& courierName)
        : Parcel(type, id, sender, receiver, sendTime, receiveTime, status, desc, weight, courierName) {}
    double getPrice() const override { return 8.0 * getWeight(); }
};

class BookParcel : public Parcel {
public:
    BookParcel() = default;
    BookParcel(const std::string& id, const std::string& sender, const std::string& receiver,
               double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::BOOK) {}
    BookParcel(ParcelType type, const std::string& id, const std::string& sender, const std::string& receiver,
                 time_t sendTime, time_t receiveTime, ParcelStatus status, const std::string& desc, double weight,    
                 const std::string& courierName)
        : Parcel(type, id, sender, receiver, sendTime, receiveTime, status, desc, weight, courierName) {}
    double getPrice() const override { return 2.0 * getWeight(); }  // weight 在此表示数量
};