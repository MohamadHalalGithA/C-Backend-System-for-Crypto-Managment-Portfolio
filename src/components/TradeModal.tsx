import { useMemo, useState, useEffect } from "react";
import { Dialog, DialogContent, DialogHeader, DialogTitle } from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { useCreateTransaction } from "@/hooks/usePortfolio";
import { useToast } from "@/hooks/use-toast";
import { TrendingUp, TrendingDown } from "lucide-react";
import type { Asset } from "@/lib/types";

interface TradeModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  type: "buy" | "sell";
  assets: Asset[];
}

export function TradeModal({ open, onOpenChange, type, assets }: TradeModalProps) {
  const [asset, setAsset] = useState("");
  const [amount, setAmount] = useState("");
  const [isSubmitting, setIsSubmitting] = useState(false);

  const createTransaction = useCreateTransaction();
  const { toast } = useToast();

  const isBuy = type === "buy";

  const selectedAsset = useMemo(
    () => assets.find((a) => a.symbol === asset),
    [assets, asset]
  );

  // Price is ALWAYS taken from mock data (selectedAsset.current_price)
  const priceNum = selectedAsset?.current_price ?? 0;

  // Reset fields whenever the modal closes
  useEffect(() => {
    if (!open) {
      setAsset("");
      setAmount("");
      setIsSubmitting(false);
    }
  }, [open]);

  const handleAssetChange = (symbol: string) => {
    setAsset(symbol);
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    const amountNum = parseFloat(amount);

    if (!asset) {
      toast({
        title: "Select an asset",
        description: "Please choose an asset before submitting.",
        variant: "destructive",
      });
      return;
    }

    if (!selectedAsset) {
      toast({
        title: "Invalid asset",
        description: "That asset wasn't found in the portfolio list.",
        variant: "destructive",
      });
      return;
    }

    if (isNaN(amountNum) || amountNum <= 0) {
      toast({
        title: "Invalid quantity",
        description: "Quantity must be greater than 0.",
        variant: "destructive",
      });
      return;
    }

    if (type === "sell" && amountNum > selectedAsset.quantity) {
      toast({
        title: "Insufficient quantity",
        description: `You only have ${selectedAsset.quantity} ${asset}`,
        variant: "destructive",
      });
      return;
    }

    setIsSubmitting(true);
    try {
      await createTransaction.mutateAsync({
        type,
        asset,
        amount: amountNum,
        price: priceNum, // FIXED
        total: amountNum * priceNum,
        timestamp: new Date().toISOString(),
        status: "pending",
      });

      toast({
        title: `${isBuy ? "Buy" : "Sell"} order placed`,
        description: `${amountNum} ${asset} @ $${priceNum.toFixed(2)}`,
      });

      onOpenChange(false);
    } catch (err: any) {
      toast({ title: "Transaction failed", description: err.message, variant: "destructive" });
    } finally {
      setIsSubmitting(false);
    }
  };

  const showTotal = !!selectedAsset && amount && !isNaN(parseFloat(amount));
  const totalValue = (parseFloat(amount || "0") * priceNum) || 0;

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="cyber-card border-border sm:max-w-md">
        <DialogHeader>
          <DialogTitle className={`flex items-center gap-2 text-xl ${isBuy ? "text-green-400" : "text-red-400"}`}>
            {isBuy ? <TrendingUp size={24} /> : <TrendingDown size={24} />}
            {isBuy ? "Buy Asset" : "Sell Asset"}
          </DialogTitle>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4 mt-4">
          <div className="space-y-2">
            <Label htmlFor="asset">Asset</Label>
            <Select value={asset} onValueChange={handleAssetChange}>
              <SelectTrigger className="cyber-input">
                <SelectValue placeholder="Select asset" />
              </SelectTrigger>
              <SelectContent>
                {assets.map((a) => (
                  <SelectItem key={a.id} value={a.symbol}>
                    <span className="font-bold">{a.symbol}</span> - {a.name}
                    {type === "sell" && (
                      <span className="text-muted-foreground ml-2">({a.quantity} available)</span>
                    )}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>

          <div className="space-y-2">
            <Label htmlFor="amount">Quantity</Label>
            <Input
              id="amount"
              type="number"
              step="any"
              min="0"
              placeholder="0.00"
              value={amount}
              onChange={(e) => setAmount(e.target.value)}
              className="cyber-input"
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="price">Price per unit ($)</Label>
            <Input
              id="price"
              type="text"
              className="cyber-input"
              value={selectedAsset ? `${priceNum}` : ""}
              placeholder={selectedAsset ? "" : "Select an asset to view price"}
              disabled
              readOnly
            />
            <p className="text-xs text-muted-foreground">
              Market Price based on mock data
            </p>
          </div>

          {showTotal && (
            <div className="p-3 rounded border border-border/50 bg-card/50">
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">Total</span>
                <span className="font-bold text-foreground">${totalValue.toFixed(2)}</span>
              </div>
            </div>
          )}

          <Button
            type="submit"
            disabled={isSubmitting}
            className={`w-full ${isBuy ? "bg-green-600 hover:bg-green-700" : "bg-red-600 hover:bg-red-700"} text-white`}
          >
            {isSubmitting ? "Processing..." : `${isBuy ? "Buy" : "Sell"} ${asset || "Asset"}`}
          </Button>
        </form>
      </DialogContent>
    </Dialog>
  );
}
