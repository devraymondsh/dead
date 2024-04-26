fun main() {
	var line = readlnOrNull()
	val exchange = Exchange(mutableListOf(), mutableListOf())

	// Reading stdin until it hits the EOF character.
	while (line != null) {
		// Breaking on empty line for the purpose of debugging in Intellij Idea. Feel free to comment this line.
		if (line.isEmpty()) break

		val row = line.split(',', ignoreCase = true)
		// Ignoring invalid input formats.
		if (row.size != 4) {
			System.err.println(
				"Invalid order format received: `$line`.\n"
						+ "The correct format: `order-id, side, price, quantity`."
			)
			continue
		}

		// Processing the order which is the actual logic.
		exchange.processOrder(
			Order(
				id = row[0],
				price = row[2].toUInt(),
				quantity = row[3].toUInt(),
				type = if (row[1].startsWith("B", ignoreCase = true)) OrderType.Bid else OrderType.Ask,
			)
		)

		line = readlnOrNull()
	}

	// Printing the order book to a human-readable form.
	exchange.printOrderBook()
}
