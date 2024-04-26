import kotlin.math.max
import kotlin.math.min
import java.text.DecimalFormat

// Inserts the order while keeping the list sorted based on the price.
fun MutableList<Order>.insertionSort(order: Order): List<Order> {
	// Falling back to 0 if the list empty.
	var index = 0
	if (this.isNotEmpty()) {
		when (order.type) {
			// Sorting bid prices descending
			OrderType.Bid -> while (this[index].price >= order.price) {
				index += 1
				if (index == this.lastIndex + 1) break
			}

			// Sorting ask prices ascending
			OrderType.Ask -> while (this[index].price <= order.price) {
				index += 1
				if (index == this.lastIndex + 1) break
			}
		}
	}

	this.add(index, order)
	return this
}

class Exchange(private val bids: MutableList<Order>, private val asks: MutableList<Order>) {
	private fun printTrade(bid: Order, ask: Order, amount: UInt) {
		println("trade ${bid.id},${ask.id},${ask.price},$amount")
	}

	// Starting the process. This is where the actual logic happens.
	fun processOrder(order: Order) {
		val isBid = order.type == OrderType.Bid
		val side = if (isBid) this.bids else this.asks
		val otherSide = if (isBid) this.asks else this.bids
		if (otherSide.isEmpty()) {
			side.insertionSort(order)
			return
		}

		val mutIterator = otherSide.listIterator()
		mutIterator.forEach {
			// Determining trading compatibility based on the order type.
			val canMatch= if (isBid) (it.price <= order.price) else (it.price >= order.price)
			if (canMatch) {
				val amount = min(order.quantity, it.quantity)
				order.quantity -= amount
				it.quantity -= amount
				// Telling the iterator to mutate the underlying list.
				mutIterator.set(it)

				// Switching the order of `it` and `order` based on the processing order type.
				if (isBid) printTrade(order, it, amount)
				else printTrade(it, order, amount)

				// Removing the order completely when the quantities are fully used.
				if (it.quantity == 0u) mutIterator.remove()
				// This is where to stop when there's enough demand.
				if (order.quantity == 0u) return
			} else {
				// Inserting the order and returning when there's no matching demands. This is only safe
				// to do because both bids and asks are sorted.
				side.insertionSort(order)
				return
			}
		}
	}

	// Formatting quantity for printing to a human-readable format considering appropriate paddings.
	private fun formatQuantity(order: Order?): String {
		// Apparently, DecimalFormat can't accept an unsigned integer as a number.
		// However, we can safely cast from UInt to Int considering the context.
		return if (order != null)
			DecimalFormat("###,###,###")
				.format(order.quantity.toInt())
				// Replacing No-break space with comma
				.replace('\u00A0', ',')
				.padStart(11, ' ')
		else " ".repeat(11)
	}

	// Formatting price for printing to a human-readable format considering appropriate paddings.
	private fun formatPrice(order: Order?): String {
		return order?.price?.toString()?.padStart(6, ' ') ?: " ".repeat(6)
	}


	// Prints the whole order book like below:
	// Equal length: BID | ASK
	// More bids than asks: BID |
	// More asks than bids: <paddings> | ASK
	fun printOrderBook() {
		for (i in 0..max(this.bids.lastIndex, this.asks.lastIndex)) {
			val bid = this.bids.getOrNull(i)
			val ask = this.asks.getOrNull(i)
			println(
				"${formatQuantity(bid)} ${formatPrice(bid)} | ${formatPrice(ask)} ${formatQuantity(ask)}"
			)
		}
	}
}