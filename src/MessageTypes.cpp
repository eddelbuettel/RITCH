#include "MessageTypes.h"

/**
 * @brief      Converts 2 bytes from a buffer in big endian to an unsigned integer
 * 
 * @param      buf   The buffer as a pointer to an array of unsigned chars
 *
 * @return     The converted unsigned integer
 */
unsigned int get2bytes(unsigned char* buf) {
  return __builtin_bswap16(*reinterpret_cast<uint16_t*>(&buf[0]));
}

/**
 * @brief      Converts 4 bytes from a buffer in big endian to an unsigned integer
 * 
 * @param      buf   The buffer as a pointer to an array of unsigned chars
 *
 * @return     The converted unsigned integer
 */
unsigned int get4bytes(unsigned char* buf) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(&buf[0]));
}

/**
 * @brief      Converts 6 bytes from a buffer in big endian to an unsigned long long
 * 
 * @param      buf   The buffer as a pointer to an array of unsigned chars
 *
 * @return     The converted unsigned long long
 */
unsigned long long int get6bytes(unsigned char* buf) {
  return (__builtin_bswap64(*reinterpret_cast<uint64_t*>(&buf[0])) & 0xFFFFFFFFFFFF0000) >> 16;
}

/**
 * @brief      Converts 8 bytes from a buffer in big endian to an unsigned long long
 * 
 * @param      buf   The buffer as a pointer to an array of unsigned chars
 *
 * @return     The converted unsigned long long
 */
unsigned long long int get8bytes(unsigned char* buf) {
  return __builtin_bswap64(*reinterpret_cast<uint64_t*>(&buf[0]));
}

/**
 * @brief      Counts the number of valid messages for this messagetype, given a count-vector
 *
 * @param[in]  count  The count vecotr
 *
 * @return     The Number of valid messages.
 */
unsigned long long MessageType::countValidMessages(std::vector<unsigned long long> count) { 
  unsigned long long total = 0;
  for (int typePos : typePositions) {
      total += count[typePos];
  }
  return total; 
}

// virtual functions of the class MessageType, will be overloaded by the other classes
bool MessageType::loadMessages(unsigned char* buf) { return bool(); }
Rcpp::DataFrame MessageType::getDF() { return Rcpp::DataFrame(); }
void MessageType::reserve(unsigned long long size) {}


/**
 * @brief      Sets the boundaries, i.e., which message numbers should be actually parsed
 *
 * @param[in]  startMsgCount  The start message count, i.e., what is the first message to be parsed,
 *                             defaults to 0 (the first message)
 * @param[in]  endMsgCount    The end message count, i.e., what is the last message to be parsed,
 *                             defaults to +Inf (the last message)
 */
void MessageType::setBoundaries(unsigned long long startMsgCount, unsigned long long endMsgCount) {
  this->startMsgCount = startMsgCount;
  this->endMsgCount   = endMsgCount;
}

// ################################################################################
// ################################ ORDERS ########################################
// ################################################################################

/**
 * @brief      Loads the information from an order into the class, either of type 'A' or 'F'
 *
 * @param      buf   The buffer
 *
 * @return     false if the boundaries are broken (all necessary messages are already loaded), 
 *              thus the loading process can be aborted, otherwise true
 */
bool Orders::loadMessages(unsigned char* buf) {

  // first check if this is the wrong message
  bool rightMessage = false;
  for (unsigned char type : validTypes) {
    rightMessage = rightMessage || buf[0] == type;
  }
  
  // if the message is of the wrong type, terminate here, but continue with the next message
  if (!rightMessage) return true;
  
  // if the message is out of bounds (i.e., we dont want to collect it yet!)
  if (messageCount < startMsgCount) {
    ++messageCount;
    return true;
  }

  // if the message is out of bounds (i.e., we dont want to collect it ever, 
  // thus aborting the information gathering (return false!))
  // no need to iterate over all the other messages.
  if (messageCount > endMsgCount) return false;
  
  // else, we can continue to parse the message to the content vectors
  type.push_back(           buf[0] );
  locateCode.push_back(     get2bytes(&buf[1]) );
  trackingNumber.push_back( get2bytes(&buf[3]) );
  timestamp.push_back(      get6bytes(&buf[5]) );
  orderRef.push_back(       get8bytes(&buf[11]) );
  buy.push_back(            buf[19] == 'B' );
  shares.push_back(         get4bytes(&buf[20]) );

  // 8 characters make up the stockname
  std::string stock_string;
  const unsigned char white = ' ';
  for (unsigned int i = 0; i < 8U; ++i) {
    if (buf[24 + i] != white) stock_string += buf[24 + i];
  }
  stock.push_back( stock_string );
  
  price.push_back( (double) get4bytes(&buf[32]) / 10000.0 );
  
  // 4 characters make up the MPID-string (if message type 'F')
  std::string mpid_string = "";
  if (buf[0] == 'F') { // type 'F' is an MPID order
    for (unsigned int i = 0; i < 4U; ++i) {
      if (buf[36 + i] != white) mpid_string += buf[36 + i];
    }
  }
  mpid.push_back( mpid_string );
  
  // increase the number of this message type
  ++messageCount;
  return true;
}

/**
 * @brief      Converts the stored information into an Rcpp::DataFrame
 *
 * @return     The Rcpp::DataFrame
 */
Rcpp::DataFrame Orders::getDF() {

  Rcpp::DataFrame df = Rcpp::DataFrame::create(
    Rcpp::Named("msg_type")        = type,
    Rcpp::Named("locate_code")     = locateCode,
    Rcpp::Named("tracking_number") = trackingNumber,
    Rcpp::Named("timestamp")       = timestamp,
    Rcpp::Named("order_ref")       = orderRef,
    Rcpp::Named("buy")             = buy,
    Rcpp::Named("shares")          = shares,
    Rcpp::Named("stock")           = stock,
    Rcpp::Named("price")           = price,
    Rcpp::Named("mpid")            = mpid
  );
  
  return df;
}

/**
 * @brief      Reserves the sizes of the content vectors (allows for faster code-execution)
 *
 * @param[in]  size  The size which should be reserved
 */
void Orders::reserve(unsigned long long size) {
  type.reserve(size);
  locateCode.reserve(size);
  trackingNumber.reserve(size);
  timestamp.reserve(size);
  orderRef.reserve(size);
  buy.reserve(size);
  shares.reserve(size);
  stock.reserve(size);
  price.reserve(size);
  mpid.reserve(size);
}


// ################################################################################
// ################################ Trades ########################################
// ################################################################################

/**
 * @brief      Loads the information from an trades into the class, either of type 'P', 'Q', or 'B'
 *
 * @param      buf   The buffer
 *
 * @return     false if the boundaries are broken (all necessary messages are already loaded), 
 *              thus the loading process can be aborted, otherwise true
 */
bool Trades::loadMessages(unsigned char* buf) {

  // first check if this is the wrong message
  bool wrongMessage = false;
  for (unsigned char type : validTypes) {
    wrongMessage = wrongMessage || buf[0] == type;
  }
  
  // if the message is of the wrong type, terminate here, but continue with the next message
  if (!wrongMessage) return true;
  
  // if the message is out of bounds (i.e., we dont want to collect it yet!)
  if (messageCount < startMsgCount) {
    ++messageCount;
    return true;
  }

  // if the message is out of bounds (i.e., we dont want to collect it ever, 
  // thus aborting the information gathering (return false!))
  // no need to iterate over all the other messages.
  if (messageCount > endMsgCount) return false;
  
  // else, we can continue to parse the message to the content vectors
  type.push_back(           buf[0] );
  locateCode.push_back(     get2bytes(&buf[1]) );
  trackingNumber.push_back( get2bytes(&buf[3]) );
  timestamp.push_back(      get6bytes(&buf[5]) );
  
  std::string stock_string;
  const unsigned char white = ' ';

  switch (buf[0]) {
    case 'P':
      orderRef.push_back(get8bytes(&buf[11]) );
      buy.push_back(     buf[19] == 'B' );
      shares.push_back(  get4bytes(&buf[20]) );

      // 8 characters make up the stockname
      for (unsigned int i = 0; i < 8U; ++i) {
        if (buf[24 + i] != white) stock_string += buf[24 + i];
      }
      stock.push_back(       stock_string );
      price.push_back(       (double) get4bytes(&buf[32]) / 10000.0 );
      matchNumber.push_back( get8bytes(&buf[36]) );
      // empty assigns
      crossType.push_back(' ');
      break;

    case 'Q':
      shares.push_back(get4bytes(&buf[11]));

      for (unsigned int i = 0; i < 8U; ++i) {
        if (buf[19 + i] != white) stock_string += buf[19 + i];
      }
      stock.push_back(       stock_string );
      price.push_back(       (double) get4bytes(&buf[27]) / 10000.0 ); // price = cross-price!
      matchNumber.push_back( get8bytes(&buf[31]) );
      crossType.push_back(   buf[39] );

      orderRef.push_back( 0ULL );
      buy.push_back(      false );
      break;

    case 'B': 
      matchNumber.push_back( get8bytes(&buf[11]) );
      // empty assigns
      orderRef.push_back(  0ULL );
      buy.push_back(       false );
      shares.push_back(    0ULL );
      stock.push_back(     "" );
      price.push_back(     0.0 );
      crossType.push_back( ' ' );
      break;

    default:
      Rcpp::Rcout << "Unkown Type: " << buf[0] << "\n";
      break;

  }

  // increase the number of this message type
  ++messageCount;
  return true;
}

/**
 * @brief      Converts the stored information into an Rcpp::DataFrame
 *
 * @return     The Rcpp::DataFrame
 */
Rcpp::DataFrame Trades::getDF() {

  Rcpp::DataFrame df = Rcpp::DataFrame::create(
    Rcpp::Named("msg_type")        = type,
    Rcpp::Named("locate_code")     = locateCode,
    Rcpp::Named("tracking_number") = trackingNumber,
    Rcpp::Named("timestamp")       = timestamp,
    Rcpp::Named("order_ref")       = orderRef,
    Rcpp::Named("buy")             = buy,
    Rcpp::Named("shares")          = shares,
    Rcpp::Named("stock")           = stock,
    Rcpp::Named("price")           = price,
    Rcpp::Named("match_number")    = matchNumber,
    Rcpp::Named("cross_type")      = crossType
  );
  
  return df;
}

/**
 * @brief      Reserves the sizes of the content vectors (allows for faster code-execution)
 *
 * @param[in]  size  The size which should be reserved
 */
void Trades::reserve(unsigned long long size) {
  type.reserve(size);
  locateCode.reserve(size);
  trackingNumber.reserve(size);
  timestamp.reserve(size);
  orderRef.reserve(size);
  buy.reserve(size);
  shares.reserve(size);
  stock.reserve(size);
  price.reserve(size);
  matchNumber.reserve(size);
  crossType.reserve(size);
}


// ################################################################################
// ################################ Modifications #################################
// ################################################################################

/**
 * @brief      Loads the information from an trades into the class, either of type 'P', 'Q', or 'B'
 *
 * @param      buf   The buffer
 *
 * @return     false if the boundaries are broken (all necessary messages are already loaded), 
 *              thus the loading process can be aborted, otherwise true
 */
bool Modifications::loadMessages(unsigned char* buf) {

  // first check if this is the wrong message
  bool wrongMessage = false;
  for (unsigned char type : validTypes) {
    wrongMessage = wrongMessage || buf[0] == type;
  }
  
  // if the message is of the wrong type, terminate here, but continue with the next message
  if (!wrongMessage) return true;

  // if the message is out of bounds (i.e., we dont want to collect it yet!)
  if (messageCount < startMsgCount) {
    ++messageCount;
    return true;
  }

  // if the message is out of bounds (i.e., we dont want to collect it ever, 
  // thus aborting the information gathering (return false!))
  // no need to iterate over all the other messages.
  if (messageCount > endMsgCount) return false;
  
  // else, we can continue to parse the message to the content vectors
  type.push_back(           buf[0] );
  locateCode.push_back(     get2bytes(&buf[1]) );
  trackingNumber.push_back( get2bytes(&buf[3]) );
  timestamp.push_back(      get6bytes(&buf[5]) );
  orderRef.push_back(       get8bytes(&buf[11]) );
  
  switch (buf[0]) {
    case 'E':
      shares.push_back(      get4bytes(&buf[19]) ); // executed shares
      matchNumber.push_back( get8bytes(&buf[23]) );
      // empty assigns
      printable.push_back(   'N' );
      price.push_back(       0.0 );
      newOrderRef.push_back( 0ULL );
      break;

    case 'C':
      shares.push_back(      get4bytes(&buf[19]) ); // executed shares
      matchNumber.push_back( get8bytes(&buf[23]) );
      printable.push_back(   buf[31] );
      price.push_back(       (double) get4bytes(&buf[32]) / 10000.0 );
      // empty assigns
      newOrderRef.push_back( 0ULL );
      break;

    case 'X':
      shares.push_back(      get4bytes(&buf[19]) ); // cancelled shares
      // empty assigns
      matchNumber.push_back( 0ULL);
      printable.push_back(   false );
      price.push_back(       0.0 );
      newOrderRef.push_back( 0ULL );
      break;

    case 'D':
      // empty assigns
      shares.push_back(      0ULL); 
      matchNumber.push_back( 0ULL);
      printable.push_back(   false );
      price.push_back(       0.0 );
      newOrderRef.push_back( 0ULL );
      break;

    case 'U':
      // the order ref is the original order reference, 
      // the new order reference is the new order reference
      newOrderRef.push_back( get8bytes(&buf[19]) );
      shares.push_back(      get4bytes(&buf[27]) );
      price.push_back(       (double) get4bytes(&buf[31]) / 10000.0 );
      // empty assigns
      matchNumber.push_back( 0ULL);
      printable.push_back(   false );
      break;

    default:
      Rcpp::Rcout << "Unkown message type: " << buf[0] << "\n";
    break;
  }

  // increase the number of this message type
  ++messageCount;
  return true;
}

/**
 * @brief      Converts the stored information into an Rcpp::DataFrame
 *
 * @return     The Rcpp::DataFrame
 */
Rcpp::DataFrame Modifications::getDF() {

  Rcpp::DataFrame df = Rcpp::DataFrame::create(
    Rcpp::Named("msg_type")        = type,
    Rcpp::Named("locate_code")     = locateCode,
    Rcpp::Named("tracking_number") = trackingNumber,
    Rcpp::Named("timestamp")       = timestamp,
    Rcpp::Named("order_ref")       = orderRef,
    Rcpp::Named("shares")          = shares,
    Rcpp::Named("match_number")    = matchNumber,
    Rcpp::Named("printable")       = printable,
    Rcpp::Named("price")           = price,
    Rcpp::Named("new_order_ref")   = newOrderRef
  );
  
  return df;
}

/**
 * @brief      Reserves the sizes of the content vectors (allows for faster code-execution)
 *
 * @param[in]  size  The size which should be reserved
 */
void Modifications::reserve(unsigned long long size) {
  type.reserve(size);
  locateCode.reserve(size);
  trackingNumber.reserve(size);
  timestamp.reserve(size);
  orderRef.reserve(size);
  shares.reserve(size);
  matchNumber.reserve(size);
  printable.reserve(size);
  price.reserve(size);
  newOrderRef.reserve(size);
}
