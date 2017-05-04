/*****************************************************************************/
/**
* \file	TimeoutSerialThread.hpp
*
* \author	Per Johansson
*
* Copyright &copy; Maquet Critical Care AB, Sweden
*
******************************************************************************/
#pragma once

#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/asio.hpp>
#include "ThreadSafeQueue.hpp"
#include <atomic>

/****************************************************************************/
/**
* \brief Exception. Thrown if timeout occurs.
*
*****************************************************************************/
class timeout_exception : public std::runtime_error
{
public:
	/*****************************************************************************/
	/**
	* \brief The timeout exception.
	*
	* \param arg Argument string.
	*
	******************************************************************************/
	explicit timeout_exception(const std::string& arg) : runtime_error(arg)
	{
	}
};

/*****************************************************************************/
/**
* \brief Serial port class with timeout on read operations.
*
******************************************************************************/
class TimeoutSerialThread : private boost::noncopyable
{
public:

	/****************************************************************************/
	/**
	* \brief Constructor, used when writing to serial device.
	*
	* \param devname Serial device.
	* \param baudrate Baudrate.
	* \param opt_parity Parity. Default: none.
	* \param opt_csize Nr of databits. Default: 8.
	* \param opt_flow Flow control. Default: none.
	* \param opt_stop Nr of stopbits. Default: 1.
	*
	******************************************************************************/
	TimeoutSerialThread(const std::string& devname, std::uint32_t baudrate,
		boost::asio::serial_port_base::parity opt_parity =
		boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none),
		boost::asio::serial_port_base::character_size opt_csize =
		boost::asio::serial_port_base::character_size(8),
		boost::asio::serial_port_base::flow_control opt_flow =
		boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none),
		boost::asio::serial_port_base::stop_bits opt_stop =
		boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));


	/*****************************************************************************/
	/**
	* \brief Constructor, used when reading from serial device.
	*
	* \param delim Message delimiter.
	* \param queue Queue for received messages.
	* \param devname Serial device.
	* \param baudrate Baudrate.
	* \param opt_parity Parity. Default: none.
	* \param opt_csize Nr of databits. Default: 8.
	* \param opt_flow Flow control. Default: none.
	* \param opt_stop Nr of stopbits. Default: 1.
	*
	******************************************************************************/
	TimeoutSerialThread(const char *delim, ThreadSafeQueue<std::string *> *queue, const std::string& devname, std::uint32_t baudrate,
		boost::asio::serial_port_base::parity opt_parity =
		boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none),
		boost::asio::serial_port_base::character_size opt_csize =
		boost::asio::serial_port_base::character_size(8),
		boost::asio::serial_port_base::flow_control opt_flow =
		boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none),
		boost::asio::serial_port_base::stop_bits opt_stop =
		boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));


	/****************************************************************************/
	/**
	* Destructor
	*
	*****************************************************************************/
	~TimeoutSerialThread();


	/****************************************************************************/
	/**
	* \brief Open the serial device.
	*
	* \return true upon success.
	*
	*****************************************************************************/
	bool open();


	/****************************************************************************/
	/**
	* \brief Check if serial device is open.
	*
	* \return true if serial device is open.
	*
	*****************************************************************************/
	bool isOpen() const;


	/****************************************************************************/
	/**
	* \brief Close the serial device.
	*
	* \throws boost::system::system_error if any error
	*
	*****************************************************************************/
	void close();


	/****************************************************************************/
	/**
	* \brief Set the timeout on read/write operations.
	*
	* To disable the timeout, call setTimeout(boost::posix_time::seconds(0)).
	*
	* \param t Timeout in seconds.
	*
	*****************************************************************************/
	void setTimeout(const boost::posix_time::time_duration& t);


	/****************************************************************************/
	/**
	* \brief Write data
	*
	* \param data Array of char to be sent through the serial device.
	* \param size Array size.
	* 
	* \throws boost::system::system_error if any error
	*
	*****************************************************************************/
	void write(const char *data, size_t size);


	/****************************************************************************/
	/**
	* \brief Write data
	*
	* \param data Data to be sent through the serial device.
	*
	* \throws boost::system::system_error if any error
	*
	*****************************************************************************/
	void write(const std::vector<char>& data);


	/****************************************************************************/
	/**
	* \brief Write a string. Can be used to send ASCII data to the serial device.
	*
	* To send binary data, use write()
	*
	* \param s String to send.
	*
	* \throws boost::system::system_error if any error
	*
	*****************************************************************************/
	void writeString(const std::string& s);


	/****************************************************************************/
	/**
	* \brief Serial device read thread.
	*
	* Read lines/messages from serial device, post a string with the received data
	* into the message queue. The line delimiter is removed from the string.
	*
	* Can only be used if the user is sure that the serial device will not
	* send binary data.
	*
	* \throw boost::system::system_error if any error
	* \throw timeout_exception in case of timeout
	*
	*****************************************************************************/
	void operator()();


	/*****************************************************************************/
	/**
	* \brief Returns true if the Serial thread is alive.
	*
	* \return true/false.
	*
	******************************************************************************/
	bool isAlive();


	/*****************************************************************************/
	/**
	* \brief Set stop flag for the Serial thread.
	*
	******************************************************************************/
	void requestStop();

private:

	/*****************************************************************************/
	/**
	* \brief Asynchronous read.
	*
	* \param delim Message delimiter.
	*
	******************************************************************************/
	void asyncReadUntil(const std::string& delim);


	/*****************************************************************************/
	/**
	* \brief Returns true if the Serial thread is to be stopped.
	*
	* \return true/false.
	*
	******************************************************************************/
	bool isStopRequested();


	/*****************************************************************************/
	/**
	* \brief Sets the alive status of the Serial thread.
	*
	* \param alive Alive status, true or false.
	*
	******************************************************************************/
	void setAlive(const bool alive);


	/****************************************************************************/
	/**
	* \brief Callack called either when the read timeout is expired or canceled.
	*
	* If called because timeout expired, sets result to resultTimeoutExpired.
	*
	* \param error Boost error code.
	*
	*****************************************************************************/
	void timeoutExpired(const boost::system::error_code& error);


	/****************************************************************************/
	/**
	* \brief Callback called either if a read completed or read error occured.
	*
	* If called because of read complete, sets result to resultSuccess.
	* If called because read error, sets result to resultError.
	*
	* \param error Boost error code.
	* \param bytesTransferred Nr of bytes read from serial device.
	*
	*****************************************************************************/
	void readCompleted(const boost::system::error_code& error,
		const size_t bytesTransferred);


	/*****************************************************************************/
	/**
	* \brief Terminate Serial thread.
	*
	******************************************************************************/
	void cleanup();


	/**
	* Message timeout settings
	*/
	enum Settings
	{
		MESSAGE_TIMEOUT = 360,	///< Timeout in seconds.
	};

	/**
	* Possible outcome of a read. Set by callbacks, read from main code
	*/
	enum ReadResult
	{
		resultInProgress,		///< Waiting for data.
		resultSuccess,			///< Writing data to queue.
		resultError,			///< Error. Terminate thread.
		resultTimeoutExpired	///< Check for stopRequested or message timeout.
	};

	boost::asio::io_service io_;					///< Io service object.
	boost::asio::serial_port port_;					///< Serial port object.
	std::string devname_;							///< Serial device.
	std::uint32_t baudrate_;						///< Baudrate.
	boost::asio::serial_port_base::parity opt_parity_;	///< Parity.
	boost::asio::serial_port_base::character_size opt_csize_;	///< Nr of databits.
	boost::asio::serial_port_base::flow_control opt_flow_;		///< Flow control.
	boost::asio::serial_port_base::stop_bits opt_stop_;			///< Nr of stopbits.
	boost::asio::deadline_timer timer_;							///< Timer for timeout.
	boost::posix_time::time_duration timeout_;					///< Read/write timeout.
	boost::asio::streambuf readData_;				///< Holds eventual read but not consumed data.
	enum ReadResult result_;						///< Read status. Used by read with timeout.
	size_t bytesTransferred_;						///< Nr of bytes read from serial device.
	std::string delim_;								///< Message delimiter.
	ThreadSafeQueue<std::string *> *queue_;			///< Queue for received messages.
	std::mutex mutex_;								///< Handles synchronization with DataSource thread.
	std::atomic<bool> isAlive_;						///< True if the Serial thread is alive.
	std::atomic<bool> stopRequested_;				///< Request to terminate Serial thread.
};

