enum class OrderType { Bid, Ask }

data class Order(
	val id: String,
	// Buyer or seller
	val type: OrderType,
	// Maximum value possible: `999,999`.
	val price: UInt,
	// Maximum value possible: `999,999,999`.
	var quantity: UInt,
)