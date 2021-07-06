#include "pch.h"
#include "BluetoothLowEnergyApi.h"

// Namespaces
using namespace std;
using namespace winrt;
using namespace winrt::Windows::Devices::Radios;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	The connected charactrristic (used locally). </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedCharacteristic : Characteristic
{
	ConnectedCharacteristic()
	{
		isService = false;
	}
	/// <summary>	The characteristic. </summary>
	GattCharacteristic characteristic = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	The connected service (used locally). </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedService : Characteristic
{
	ConnectedService()
	{
		isService = true;
	}
	/// <summary>	The characteristic. </summary>
	GattDeviceService service = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A connected subscription. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedSubscription
{
	/// <summary>	The characteristic. </summary>
	ConnectedCharacteristic* characteristic = nullptr;
	/// <summary>	The revoker. </summary>
	GattCharacteristic::ValueChanged_revoker revoker;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A connected device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedDevice : Device
{
	/// <summary>	The le development. </summary>
	BluetoothLEDevice leDev = nullptr;
	/// <summary>	The result. </summary>
	GattDeviceServicesResult result = nullptr;
	/// <summary>	The revoker. </summary>
	BluetoothLEDevice::ConnectionStatusChanged_revoker revoker;
	/// <summary>	The subscriptions. </summary>
	list<ConnectedSubscription*> subscriptions;
	/// <summary>	The characteristics. </summary>
	list<ConnectedCharacteristic*> characteristics;

	list<ConnectedService*> services;
	/// <summary>	True if is connected, false if not. </summary>
	bool isConnected = false;
	wstring ToString()
	{
		return addrToString(address) + L" " + (leDev.Name().size() ? wstring(leDev.Name()) : L"");
	}
};

fire_and_forget DisconnectDeviceAsync(ConnectedDevice* device);

/// <summary>	The connected lock. </summary>
//mutex connectedLock;
/// <summary>	The devices. </summary>
list<ConnectedDevice*> devices;

ConnectedDevice* GetDevice(uint64_t deviceAddress)
{
	try
	{
		for (ConnectedDevice* device : devices)
		{
			if (device->address == deviceAddress)
			{
				return device;
			}
		}
	}
	catch (...)
	{
		AddError("GetDevice error: exception caught", nullptr, nullptr);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connection status changed event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="dev">	The development. </param>
/// <param name="obj">	The object. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void ConnectionStatusChangedEvent(BluetoothLEDevice dev, const IInspectable& obj)
{
	try
	{
		//lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(dev.BluetoothAddress());

		if (device != nullptr && device->isConnected)
		{
			if (dev.ConnectionStatus() != BluetoothConnectionStatus::Connected)
			{
				device->isConnected = false;
				DisconnectDeviceAsync(device);
			}
			else
			{
				AddEvent(What::DEVICE_CONNECTED, device, nullptr, nullptr);
				RemoveAdvertisedDevice(device->address);
			}
		}
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connects a device asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

//void MaxPduSizeChangedEvent(GattSession dev, const IInspectable& obj)
//{
//	cout << "PDU changed to " << dev.MaxPduSize() << endl;
//}

fire_and_forget ConnectDeviceAsync(uint64_t deviceAddress)
{
	if (deviceAddress == 0) co_return;

	ConnectedDevice* device = nullptr;

	try
	{
		//lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);

		if (device != nullptr) co_return;

		device = new ConnectedDevice();
		device->address = deviceAddress;
		device->leDev = co_await BluetoothLEDevice::FromBluetoothAddressAsync(device->address);
		device->revoker = device->leDev.ConnectionStatusChanged(auto_revoke, ConnectionStatusChangedEvent);

		//GattDeviceService gds = GattDeviceService::FromIdAsync(device->leDev.DeviceId());

		// TODO : Embed gattsession in connectedDevice ? + check working
		//GattSession gatt = GattSession::FromDeviceIdAsync(device->leDev.BluetoothDeviceId()).get();
		//std::cout << "MAX PDU SIZE = "<< gatt.MaxPduSize() << endl;
		//gatt.MaxPduSizeChanged(auto_revoke, MaxPduSizeChangedEvent);
		//gatt.Close();

		/*if (device->leDev.DeviceInformation().Pairing().CanPair())
		{
			cout << "can pair" << endl;
			device->leDev.DeviceInformation().Pairing().PairAsync(DevicePairingProtectionLevel::EncryptionAndAuthentication).get();
			if (device->leDev.DeviceInformation().Pairing().IsPaired())
			{
				cout << "is paired" << endl;
			}
			else
			{
				cout << "is NOT paired" << endl;
			}
		}*/

		//

		device->result = co_await device->leDev.GetGattServicesAsync(BluetoothCacheMode::Uncached);
		device->isConnected = device->result.Status() == GattCommunicationStatus::Success;

		if (device->isConnected)
		{
			devices.push_back(device);

			// gather name Remove from advertisments
			if (!GetNameFromAdvertisment(deviceAddress, device->name))
			{
				wstring w = (wstring)device->leDev.Name();
				wcscpy_s(device->name, sizeof(device->name) / sizeof(wchar_t), w.c_str());
			}

			RemoveAdvertisedDevice(device->address);
			AddEvent(What::DEVICE_CONNECTED, device, nullptr, nullptr);

			ConnectedCharacteristic* currentC = nullptr;
			ConnectedService* currentS = nullptr;
			try
			{
				IVectorView<GattDeviceService> services = device->result.Services();
				for (GattDeviceService service : services)
				{
					if (!device->isConnected) co_return;
					{
						wstring uuidStr = (wstring)to_hstring(service.Uuid());
						//wcout << "s" << uuidStr << endl;
						currentS = new ConnectedService();
						currentS->isService = true;
						currentS->service = service;
						wcscpy_s(currentS->uuid, sizeof(currentS->uuid) / sizeof(wchar_t), uuidStr.c_str());
						device->services.push_back(currentS);
						AddEvent(What::CHARACTERISTIC_SCAN_NEW, device, currentS, nullptr);
					}

					IGattCharacteristicsResult r = co_await service.GetCharacteristicsAsync();
					IVectorView<GattCharacteristic> characteristics = r.Characteristics();

					for (GattCharacteristic characteristic : characteristics)
					{
						if (!device->isConnected) co_return;
						wstring uuidStr = (wstring)to_hstring(characteristic.Uuid());
						//wcout << "c" << uuidStr << endl;
						currentC = new ConnectedCharacteristic();
						currentC->isService = false;
						currentC->characteristic = characteristic;
						wcscpy_s(currentC->uuid, sizeof(currentC->uuid) / sizeof(wchar_t), uuidStr.c_str());
						device->characteristics.push_back(currentC);
						AddEvent(What::CHARACTERISTIC_SCAN_NEW, device, currentC, nullptr);
					}
				}
				AddEvent(What::CHARACTERISTIC_SCAN_OVER, device, nullptr, nullptr);
			}
			catch (...)
			{
				AddError("ScanCharacteristicsAsync error: exception caught", device, nullptr);
			}
		}
		else
		{
			DisconnectDeviceAsync(device);
		}
	}
	catch (...)
	{
		AddError("ConnectDeviceAsync error: exception caught", device, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connects a device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void ConnectDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return;
	try
	{
		ConnectDeviceAsync(deviceAddress);
	}
	catch (...)
	{
		AddError("ConnectDevice error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device described by device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="device">	[in,out] If non-null, the device. </param>
///
/// <returns>	True if it succeeds, false if it fails. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

fire_and_forget DisconnectDeviceAsync(ConnectedDevice* device)
{
	if (device == nullptr) co_return;
	try
	{
		for (ConnectedSubscription* sub : device->subscriptions)
		{
			try
			{
				if (device->isConnected)
				{
					GattCommunicationStatus status = co_await sub->characteristic->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);

					if (status == GattCommunicationStatus::Success)
					{
						AddEvent(What::CHARACTERISTIC_UNSUBSCRIBED, device, sub->characteristic, nullptr);
					}
					else
					{
						AddEvent(What::CHARACTERISTIC_NOT_UNSUBSCRIBED, device, sub->characteristic, nullptr);
					}
				}
				sub->characteristic->characteristic.Service().Close();
				sub->revoker.revoke();
				delete sub;
			}
			catch (...)
			{
				AddEvent(What::CHARACTERISTIC_NOT_UNSUBSCRIBED, device, sub->characteristic, nullptr);
			}
		}

		for (ConnectedCharacteristic* sub : device->characteristics)
		{
			delete sub;
			sub = NULL;
		}

		for (ConnectedService* sub : device->services)
		{
			sub->service.Close();
			sub->service = NULL;
			delete sub;
			sub = NULL;
		}

		device->subscriptions.clear();
		device->characteristics.clear();

		device->revoker.revoke();
		device->leDev.Close();
		device->leDev = NULL;

		if (device->isConnected)
		{
			AddEvent(What::DEVICE_DISCONNECTED, device, nullptr, nullptr);
		}
		else
		{
			AddEvent(What::DEVICE_LOST, device, nullptr, nullptr);
		}

		device->isConnected = false;

		devices.remove(device);
		delete device;
		device = nullptr;

		co_return;
	}
	catch (...)
	{
		AddError("DisconnectDeviceAsync error: exception caught", device, nullptr);
	}
	co_return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device described by deviceAddress. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void DisconnectDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return;
	ConnectedDevice* device = nullptr;
	try
	{
		//lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			DisconnectDeviceAsync(device);
			return;
		}
	}
	catch (...)
	{
		AddError("DisconnectDeviceAsync error: exception caught", device, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Query if 'deviceAddress' is device connected. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
///
/// <returns>	True if device connected, false if not. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool IsDeviceConnected(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return false;
	ConnectedDevice* device = nullptr;
	try
	{
		//lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			return device->isConnected;
		}
	}
	catch (...)
	{
		AddError("IsDeviceConnected error: exception caught", device, nullptr);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Functions - Characteristics (for connected devices) </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="dev">   	[in,out] If non-null, the development. </param>
/// <param name="t_uuid">	The uuid. </param>
///
/// <returns>	Null if it fails, else the found characteristic. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

ConnectedCharacteristic* FindCharacteristic(ConnectedDevice* dev, wstring t_uuid)
{
	if (dev == nullptr)return nullptr;
	try
	{
		transform(t_uuid.begin(), t_uuid.end(), t_uuid.begin(), towlower);
		const wchar_t* str = t_uuid.c_str();

		for (list<ConnectedCharacteristic*>::iterator it = dev->characteristics.begin(); it != dev->characteristics.end(); it++)
		{
			if (!wcscmp(str, (*it)->uuid))
			{
				return (*it);
			}
		}
	}
	catch (...)
	{
		AddError("FindCharacteristic error: exception caught", dev, nullptr);
		return nullptr;
	}

	AddError("FindCharacteristic error: characteristic not found !", dev, nullptr);
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device write asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
/// <param name="data">				The data. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

fire_and_forget DeviceWriteAsync(uint64_t deviceAddress, wchar_t* uuid, Data data)
{
	if (deviceAddress == 0)co_return;
	if (uuid == nullptr) co_return;
	if (data.buf == nullptr)co_return;

	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		{
			//lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);

			if (device == nullptr)
			{
				AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
				co_return;
			}
		}

		characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
			co_return;
		}

		array_view<uint8_t> av(data.buf, data.size);
		auto writer = DataWriter();
		writer.WriteBytes(av);

		auto status = co_await characteristic->characteristic.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse);
		if (status.Status() == GattCommunicationStatus::Success)
		{
			AddEvent(What::CHARACTERISTIC_WRITTEN, device, characteristic, &data);
			co_return;
		}
		else
		{
			AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
			co_return;
		}
	}
	catch (...)
	{
		AddError("DeviceWriteAsync error: exception caught", device, characteristic);
	}

	AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device write. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
/// <param name="data">				[in,out] If non-null, the data. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data)
{
	if (deviceAddress == 0)return;
	try
	{
		DeviceWriteAsync(deviceAddress, uuid, *data);
	}
	catch (...)
	{
		AddError("DeviceWrite error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device read asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

fire_and_forget DeviceReadAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)co_return;
	if (uuid == nullptr) co_return;

	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		{
			//lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);
			if (device == nullptr) co_return;
		}

		characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
			co_return;
		}

		auto val = co_await characteristic->characteristic.ReadValueAsync(BluetoothCacheMode::Uncached);

		if (val.Status() != GattCommunicationStatus::Success)
		{
			AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
			co_return;
		}

		Data data;
		data.size = val.Value().Length();

		memcpy(data.buf, val.Value().data(), data.size);
		data.buf[data.size] = '\0';

		AddEvent(What::CHARACTERISTIC_READED, device, characteristic, &data);
		co_return;
	}
	catch (...)
	{
		AddError("DeviceReadAsync error: exception caught", device, characteristic);
	}

	AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device read. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceRead(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	try
	{
		DeviceReadAsync(deviceAddress, uuid);
	}
	catch (...)
	{
		AddError("DeviceRead error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device subscribe asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////
fire_and_forget DeviceSubscribeAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0) co_return;
	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		//lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);

		if (device != nullptr)
		{
			characteristic = FindCharacteristic(device, uuid);

			if (characteristic != nullptr)
			{
				// go trough subscriptions to check if existing
				for (ConnectedSubscription* sub : device->subscriptions)
				{
					if (sub->characteristic->characteristic.Uuid() == characteristic->characteristic.Uuid())
					{
						co_return;
					}
				}

				ConnectedSubscription* sub = new ConnectedSubscription();
				sub->characteristic = characteristic;

				// TODO test
				//sub->characteristic->characteristic.ProtectionLevel(GattProtectionLevel::EncryptionRequired);

				GattCommunicationStatus status = co_await sub->characteristic->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);

				if (status == GattCommunicationStatus::Success)
				{
					sub->revoker = sub->characteristic->characteristic.ValueChanged(auto_revoke, [device, sub](const GattCharacteristic&, const GattValueChangedEventArgs& args)
						{
							try
							{
								Data cd;
								cd.size = args.CharacteristicValue().Length();
								memcpy(cd.buf, args.CharacteristicValue().data(), cd.size);
								cd.buf[cd.size] = '\0';
								AddEvent(What::CHARACTERISTIC_NOTIFICATION, device, sub->characteristic, &cd);
							}
							catch (...)
							{
								AddError("DeviceSubscribeAsync ValueChanged error: exception caught", device, sub->characteristic);
							}
							return;
						});

					device->subscriptions.push_back(sub);
					AddEvent(What::CHARACTERISTIC_SUBSCRIBED, device, characteristic, nullptr);
					co_return;
				}
				delete sub;
				sub = nullptr;
				AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
				co_return;
			}
			AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
			co_return;
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribeAsync error: exception caught", device, characteristic);
	}
	AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
}

fire_and_forget DeviceUnSubscribeAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0) co_return;
	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		//lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);

		if (device != nullptr)
		{
			characteristic = FindCharacteristic(device, uuid);

			if (characteristic != nullptr)
			{
				// go trough subscriptions to check if existing
				for (ConnectedSubscription* sub : device->subscriptions)
				{
					if (sub->characteristic->characteristic.Uuid() == characteristic->characteristic.Uuid())
					{
						GattCommunicationStatus status = co_await sub->characteristic->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);
						if (status == GattCommunicationStatus::Success)
						{
							sub->characteristic->characteristic.Service().Close();
							device->subscriptions.remove(sub);
							AddEvent(What::CHARACTERISTIC_UNSUBSCRIBED, device, characteristic, nullptr);
							co_return;
						}
					}
				}
				AddEvent(What::CHARACTERISTIC_NOT_UNSUBSCRIBED, device, characteristic, nullptr);
				co_return;
			}
			AddEvent(What::CHARACTERISTIC_NOT_UNSUBSCRIBED, device, characteristic, nullptr);
			co_return;
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribeAsync error: exception caught", device, characteristic);
	}
	AddEvent(What::CHARACTERISTIC_NOT_UNSUBSCRIBED, device, characteristic, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device characteristic subscribe. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	if (uuid == nullptr)return;
	try
	{
		DeviceSubscribeAsync(deviceAddress, uuid);
	}
	catch (...)
	{
		AddError("DeviceSubscribe error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device characteristic unsubscribe. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceUnSubscribe(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	if (uuid == nullptr)return;
	try
	{
		DeviceUnSubscribeAsync(deviceAddress, uuid);
	}
	catch (...)
	{
		AddError("DeviceUnSubscribe error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void CleanConnect()
{
	try
	{
		//lock_guard lock(connectedLock);
		if (devices.size() > 0)
		{
			for (ConnectedDevice* device : devices)
			{
				DisconnectDeviceAsync(device);
			}
			devices.clear();
		}
	}
	catch (...)
	{
		AddError("Clean error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnect all devices </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DisconnectAll()
{
	try
	{
		//lock_guard lock(connectedLock);
		for (ConnectedDevice* device : devices)
		{
			DisconnectDeviceAsync(device);
		}
		devices.clear();
	}
	catch (...)
	{
		AddError("DisconnectAll error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Get the number of devices connected </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport int16_t ConnectedCount()
{
	try
	{
		return (int16_t)devices.size();
	}
	catch (...)
	{
		AddError("ConnectedCount error: exception caught", nullptr, nullptr);
	}

	return -1;
}