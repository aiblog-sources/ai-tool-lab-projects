export type Options = {
	/**
	How many times to retry.
	*/
	limit?: number;

	/**
	Timeout in milliseconds.
	*/
	timeout?: number;
};

export interface Hooks {
	/** Called before each request. */
	beforeRequest?: () => void;
}
